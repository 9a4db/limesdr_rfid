/**
@file	Streaming.cpp
@brief	Soapy SDR + IConnection streaming interfaces.
@author Lime Microsystems (www.limemicro.com)
*/

#include "SoapyLMS7.h"
#include "LMS7002M.h"
#include <IConnection.h>
#include <SoapySDR/Formats.hpp>
#include <SoapySDR/Logger.hpp>
#include <SoapySDR/Time.hpp>
#include <thread>
#include <iostream>
#include <algorithm> //min/max
#include "ErrorReporting.h"
#include <fstream> //SID
#include <chrono> //SID
#include <thread> //SID

using namespace lime;

/*******************************************************************
 * Stream data structure
 ******************************************************************/
struct IConnectionStream
{
    std::vector<size_t> streamID;
    int direction;
    size_t elemSize;
    size_t elemMTU;
    bool enabled;

    //rx cmd requests
    bool hasCmd;
    int flags;
    long long timeNs;
    size_t numElems;
};

/*******************************************************************
 * Stream information
 ******************************************************************/
std::vector<std::string> SoapyLMS7::getStreamFormats(const int direction, const size_t channel) const
{
    std::vector<std::string> formats;
    formats.push_back(SOAPY_SDR_CF32);
    formats.push_back(SOAPY_SDR_CS12);
    formats.push_back(SOAPY_SDR_CS16);
    return formats;
}

std::string SoapyLMS7::getNativeStreamFormat(const int direction, const size_t channel, double &fullScale) const
{
    fullScale = 2048;
    return SOAPY_SDR_CS16;
}

SoapySDR::ArgInfoList SoapyLMS7::getStreamArgsInfo(const int direction, const size_t channel) const
{
    SoapySDR::ArgInfoList argInfos;

    //buffer length
    {
        SoapySDR::ArgInfo info;
        info.value = "0";
        info.key = "bufferLength";
        info.name = "Buffer Length";
        info.description = "The buffer transfer size over the link.";
        info.units = "samples";
        info.type = SoapySDR::ArgInfo::INT;
        argInfos.push_back(info);
    }

    //link format
    {
        SoapySDR::ArgInfo info;
        info.value = SOAPY_SDR_CS16;
        info.key = "linkFormat";
        info.name = "Link Format";
        info.description = "The format of the samples over the link.";
        info.type = SoapySDR::ArgInfo::STRING;
        info.options.push_back(SOAPY_SDR_CS16);
        info.options.push_back(SOAPY_SDR_CS12);
        info.optionNames.push_back("Complex int16");
        info.optionNames.push_back("Complex int12");
        argInfos.push_back(info);
    }

    return argInfos;
}

/*******************************************************************
 * Stream config
 ******************************************************************/
SoapySDR::Stream *SoapyLMS7::setupStream(
    const int direction,
    const std::string &format,
    const std::vector<size_t> &channels,
    const SoapySDR::Kwargs &args)
{
    std::unique_lock<std::recursive_mutex> lock(_accessMutex);

    //store result into opaque stream object
    auto stream = new IConnectionStream;
    stream->direction = direction;
    stream->elemSize = SoapySDR::formatToSize(format);
    stream->hasCmd = false;

    StreamConfig config;
    config.isTx = (direction == SOAPY_SDR_TX);
    config.performanceLatency = 0.5;

    //default to channel 0, if none were specified
    const std::vector<size_t> &channelIDs = channels.empty() ? std::vector<size_t>{0} : channels;

    for(size_t i=0; i<channelIDs.size(); ++i)
    {
        config.channelID = channelIDs[i];
        if (format == SOAPY_SDR_CF32) config.format = StreamConfig::STREAM_COMPLEX_FLOAT32;
        else if (format == SOAPY_SDR_CS16) config.format = StreamConfig::STREAM_12_BIT_IN_16;
        else if (format == SOAPY_SDR_CS12) config.format = StreamConfig::STREAM_12_BIT_COMPRESSED;
        else throw std::runtime_error("SoapyLMS7::setupStream(format="+format+") unsupported format");

        //optional buffer length if specified (from device args)
        const auto devArgsBufferLength = _deviceArgs.find(config.isTx?"txBufferLength":"rxBufferLength");
        if (devArgsBufferLength != _deviceArgs.end())
        {
            config.bufferLength = std::stoul(devArgsBufferLength->second);
        }

        //optional buffer length if specified (takes precedent)
        if (args.count("bufferLength") != 0)
        {
            config.bufferLength = std::stoul(args.at("bufferLength"));
        }

        //optional packets latency, 1-maximum throughput, 0-lowest latency //Comment edited by SID
        if (args.count("latency") != 0)
        {
            config.performanceLatency = std::stof(args.at("latency"));
            if(config.performanceLatency<0)
                config.performanceLatency = 0;
            else if(config.performanceLatency > 1)
                config.performanceLatency = 1;
        }

        //config.bufferLength = 512; //SID
        config.performanceLatency = 0.0; //SID

        
        //create the stream
        size_t streamID(~0);
        const int status = _conn->SetupStream(streamID, config);
        if (status != 0)
            throw std::runtime_error("SoapyLMS7::setupStream() failed: " + std::string(GetLastErrorMessage()));
        stream->streamID.push_back(streamID);
        stream->elemMTU = _conn->GetStreamSize(streamID);
        stream->enabled = false;
    }

    //calibrate these channels when activated
    for (const auto &ch : channelIDs)
    {
        _channelsToCal.emplace(direction, ch);
    }

    return (SoapySDR::Stream *)stream;
}

void SoapyLMS7::closeStream(SoapySDR::Stream *stream)
{
    std::unique_lock<std::recursive_mutex> lock(_accessMutex);
    auto icstream = (IConnectionStream *)stream;
    const auto &streamID = icstream->streamID;

    //disable stream if left enabled
    if (icstream->enabled)
    {
        for(auto i : streamID)
            _conn->ControlStream(i, false);
    }

    for(auto i : streamID)
        _conn->CloseStream(i);
}

size_t SoapyLMS7::getStreamMTU(SoapySDR::Stream *stream) const
{
    //SoapySDR::logf(SOAPY_SDR_INFO, "********In getStreamMTU**********"); //SID
    auto icstream = (IConnectionStream *)stream;
    return icstream->elemMTU;
}

int SoapyLMS7::activateStream(
    SoapySDR::Stream *stream,
    const int flags,
    const long long timeNs,
    const size_t numElems)
{
    std::unique_lock<std::recursive_mutex> lock(_accessMutex);
    auto icstream = (IConnectionStream *)stream;
    const auto &streamID = icstream->streamID;

    if (_conn->GetHardwareTimestampRate() == 0.0)
        throw std::runtime_error("SoapyLMS7::activateStream() - the sample rate has not been configured!");


    //SID put input filtering on RX
    //First read csv into a string
    //std::ifstream ifs("rx_filter_taps.csv");
    //SoapySDR::logf(SOAPY_SDR_INFO, "Read csv file");
    //std::string content( (std::istreambuf_iterator<char>(ifs) ),
    //               (std::istreambuf_iterator<char>()    ) );
    //Then make the call to enable the filter
    //writeSetting(SOAPY_SDR_RX, 0, "ENABLE_GFIR_LPF", content);
    //writeSetting(SOAPY_SDR_RX, 0, "ENABLE_GFIR_LPF", "0,120,2,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0");
    //writeSetting(SOAPY_SDR_RX, 0, "ENABLE_GFIR_LPF", "0,120,2,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1");
    //writeSetting(SOAPY_SDR_RX, 0, "ENABLE_GFIR_LPF", "0,105,2,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1");
    //writeSetting(SOAPY_SDR_RX, 0, "ENABLE_GFIR_LPF", "0,40,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0");
    //writeSetting(SOAPY_SDR_RX, 0, "ENABLE_GFIR_LPF", "0,120,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0");
    //writeSetting(SOAPY_SDR_RX, 0, "ENABLE_GFIR_LPF", "0,75,2,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1");
    //writeSetting(SOAPY_SDR_RX, 0 , "ENABLE_GFIR_LPF", "0,60,2,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1");
    

    //perform self calibration with current bandwidth settings
    //this is for the set-it-and-forget-it style of use case
    //where boards are configured, the stream is setup,
    //and the configuration is maintained throughout the run
    while (not _channelsToCal.empty())
    {
        auto dir  = _channelsToCal.begin()->first;
        auto ch  = _channelsToCal.begin()->second;
        if (dir == SOAPY_SDR_RX) getRFIC(ch)->CalibrateRx(_actualBw.at(dir).at(ch));
        if (dir == SOAPY_SDR_TX) getRFIC(ch)->CalibrateTx(_actualBw.at(dir).at(ch));
        _channelsToCal.erase(_channelsToCal.begin());
        SoapySDR::logf(SOAPY_SDR_INFO, "Calibrate in activateStream");
    }
    
    /*SoapySDR::logf(SOAPY_SDR_INFO, "Enabling TDD");

    //SoapySDR::logf(SOAPY_SDR_INFO, "Register at 0x0020 before is %x", readRegister("RFIC0", 0x0020)); //MAC
    //SoapySDR::logf(SOAPY_SDR_INFO, "Register at 0x011C before is %x", readRegister("RFIC0", 0x011C)); //PD_LOCH_T2RBUF, PD_VCO
    for (size_t channel = 0; channel < 1; channel++)
    {
        auto rfic = getRFIC(channel);
        rfic->Modify_SPI_Reg_bits(LMS7param(MAC),2);
        //SoapySDR::logf(SOAPY_SDR_INFO, "Register at 0x0020 after (MAC,2) is %x", readRegister("RFIC0", 0x0020)); //MAC
        rfic->Modify_SPI_Reg_bits(LMS7param(PD_LOCH_T2RBUF),0);
        //SoapySDR::logf(SOAPY_SDR_INFO, "Register at 0x011C after (PD_LOCH_T2RBUF,0) is %x", readRegister("RFIC0", 0x011C));
        rfic->Modify_SPI_Reg_bits(LMS7param(MAC),1);
        //SoapySDR::logf(SOAPY_SDR_INFO, "Register at 0x0020 after (MAC,1) is %x", readRegister("RFIC0", 0x0020)); //MAC
        rfic->Modify_SPI_Reg_bits(LMS7param(PD_VCO),1);
        //SoapySDR::logf(SOAPY_SDR_INFO, "Register at 0x011C after (PD_VCO,1) is %x", readRegister("RFIC0", 0x011C));
    }

    //SoapySDR::logf(SOAPY_SDR_INFO, "Register at 0x0020 after all changes is %x", readRegister("RFIC0", 0x0020)); //MAC
    //SoapySDR::logf(SOAPY_SDR_INFO, "Register at 0x011C after call changes is %x", readRegister("RFIC0", 0x011C)); //PD_LOCH_T2RBUF, PD_VCO
    */
    
    
    for (size_t channel = 0; channel < 1; channel++)
    {
        SoapySDR::logf(SOAPY_SDR_INFO, "Disabling DC corrector");
        auto rfic = getRFIC(channel);
        rfic->Modify_SPI_Reg_bits(LMS7param(DC_BYP_RXTSP), 1);
    }
    
    ///stream requests used with rx
    icstream->flags = flags;
    icstream->timeNs = timeNs;
    icstream->numElems = numElems;
    icstream->hasCmd = true;
    
    SoapySDR::logf(SOAPY_SDR_INFO, "Before sleep"); //SID
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    SoapySDR::logf(SOAPY_SDR_INFO, "After sleep"); //SID
    

    if (not icstream->enabled)
    {
        for(auto i : streamID)
        {
            int status = _conn->ControlStream(i, true);
            if(status != 0) return SOAPY_SDR_STREAM_ERROR;
        }
        icstream->enabled = true;
    }
    
    return 0;
}

int SoapyLMS7::deactivateStream(
    SoapySDR::Stream *stream,
    const int flags,
    const long long timeNs)
{
    std::unique_lock<std::recursive_mutex> lock(_accessMutex);
    auto icstream = (IConnectionStream *)stream;
    const auto &streamID = icstream->streamID;
    icstream->hasCmd = false;

    StreamMetadata metadata;
    metadata.timestamp = SoapySDR::timeNsToTicks(timeNs, _conn->GetHardwareTimestampRate());
    metadata.hasTimestamp = (flags & SOAPY_SDR_HAS_TIME) != 0;
    metadata.endOfBurst = (flags & SOAPY_SDR_END_BURST) != 0;

    if (icstream->enabled)
    {
        for(auto i : streamID)
        {
            int status = _conn->ControlStream(i, false);
            if(status != 0) return SOAPY_SDR_STREAM_ERROR;
        }
        icstream->enabled = false;
    }

    return 0;
}

/*******************************************************************
 * Stream alignment helper for multiple channels
 ******************************************************************/
static inline void fastForward(
    char *buff, size_t &numWritten, const size_t elemSize,
    const uint64_t oldHeadTime, const uint64_t desiredHeadTime)
{
    const size_t numPop = std::min<size_t>(desiredHeadTime - oldHeadTime, numWritten);
    const size_t numMove = (numWritten-numPop);
    numWritten -= numPop;
    std::memmove(buff, buff+(numPop*elemSize), numMove*elemSize);
}

int SoapyLMS7::_readStreamAligned(
    IConnectionStream *stream,
    char * const *buffs,
    size_t numElems,
    uint64_t requestTime,
    StreamMetadata &md,
    const long timeoutMs)
{
    const auto &streamID = stream->streamID;
    const size_t elemSize = stream->elemSize;
    std::vector<size_t> numWritten(streamID.size(), 0);

    for (size_t i = 0; i < streamID.size(); i += (numWritten[i]==numElems)?1:0)
    {
        size_t &N = numWritten[i];
        const uint64_t expectedTime(requestTime + N);

        int status = _conn->ReadStream(streamID[i], buffs[i]+(elemSize*N), numElems-N, timeoutMs, md);
        if (status == 0) return SOAPY_SDR_TIMEOUT;
        if (status < 0) return SOAPY_SDR_STREAM_ERROR;

        //update accounting
        const size_t elemsRead = size_t(status);
        const size_t prevN = N;
        N += elemsRead; //num written total

        //unspecified request time, set the new head condition
        if (requestTime == 0) goto updateHead;

        //good contiguous read, read again for remainder
        if (expectedTime == md.timestamp) continue;

        //request time is later, fast forward buffer
        if (md.timestamp < expectedTime)
        {
            if (prevN != 0)
            {
                SoapySDR::log(SOAPY_SDR_ERROR, "readStream() experienced non-monotonic timestamp");
                return SOAPY_SDR_CORRUPTION;
            }
            fastForward(buffs[i], N, elemSize, md.timestamp, requestTime);
            if (i == 0 and N != 0) numElems = N; //match size on other channels
            continue; //read again into the remaining buffer
        }

        //overflow in the middle of a contiguous buffer
        //fast-forward all prior channels and restart loop
        if (md.timestamp > expectedTime)
        {
            for (size_t j = 0; j < i; j++)
            {
                fastForward(buffs[j], numWritten[j], elemSize, requestTime, md.timestamp);
            }
            fastForward(buffs[i], N, elemSize, md.timestamp-prevN, md.timestamp);
            i = 0; //start over at ch0
        }

        updateHead:
        requestTime = md.timestamp;
        numElems = elemsRead;
    }

    md.timestamp = requestTime;
    return int(numElems);
}

/*******************************************************************
 * Stream API
 ******************************************************************/
int SoapyLMS7::readStream(
    SoapySDR::Stream *stream,
    void * const *buffs,
    size_t numElems,
    int &flags,
    long long &timeNs,
    const long timeoutUs)
{
    auto icstream = (IConnectionStream *)stream;

    const auto exitTime = std::chrono::high_resolution_clock::now() + std::chrono::microseconds(timeoutUs);

    //wait for a command from activate stream up to the timeout specified
    if (not icstream->hasCmd)
    {
        while (std::chrono::high_resolution_clock::now() < exitTime)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        return SOAPY_SDR_TIMEOUT;
    }

    //handle the one packet flag by clipping
    if ((flags & SOAPY_SDR_ONE_PACKET) != 0)
    {
        numElems = std::min(numElems, icstream->elemMTU);
    }

    StreamMetadata metadata;
    const uint64_t cmdTicks = ((icstream->flags & SOAPY_SDR_HAS_TIME) != 0)?SoapySDR::timeNsToTicks(icstream->timeNs, _conn->GetHardwareTimestampRate()):0;
    int status = _readStreamAligned(icstream, (char * const *)buffs, numElems, cmdTicks, metadata, timeoutUs/1000);
    if (status < 0) return status;

    //the command had a time, so we need to compare it to received time
    if ((icstream->flags & SOAPY_SDR_HAS_TIME) != 0 and metadata.hasTimestamp)
    {
        //our request time is now late, clear command and return error code
        if (cmdTicks < metadata.timestamp)
        {
            icstream->hasCmd = false;
            return SOAPY_SDR_TIME_ERROR;
        }

        //_readStreamAligned should guarantee this condition
        if (cmdTicks != metadata.timestamp)
        {
            SoapySDR::logf(SOAPY_SDR_ERROR,
                "readStream() alignment algorithm failed\n"
                "Request time = %lld, actual time = %lld",
                (long long)cmdTicks, (long long)metadata.timestamp);
            return SOAPY_SDR_STREAM_ERROR;
        }

        icstream->flags &= ~SOAPY_SDR_HAS_TIME; //clear for next read
    }

    //handle finite burst request commands
    if (icstream->numElems != 0)
    {
        //Clip to within the request size when over,
        //and reduce the number of elements requested.
        status = std::min<size_t>(status, icstream->numElems);
        icstream->numElems -= status;

        //the burst completed, done with the command
        if (icstream->numElems == 0)
        {
            icstream->hasCmd = false;
            metadata.endOfBurst = true;
        }
    }

    //output metadata
    flags = 0;
    if (metadata.endOfBurst) flags |= SOAPY_SDR_END_BURST;
    if (metadata.hasTimestamp) flags |= SOAPY_SDR_HAS_TIME;
    timeNs = SoapySDR::ticksToTimeNs(metadata.timestamp, _conn->GetHardwareTimestampRate());

    //return num read or error code
    return (status >= 0) ? status : SOAPY_SDR_STREAM_ERROR;
}

int SoapyLMS7::writeStream(
    SoapySDR::Stream *stream,
    const void * const *buffs,
    const size_t numElems,
    int &flags,
    const long long timeNs,
    const long timeoutUs)
{
    auto icstream = (IConnectionStream *)stream;
    const auto &streamID = icstream->streamID;

    //input metadata
    StreamMetadata metadata;
    metadata.timestamp = SoapySDR::timeNsToTicks(timeNs, _conn->GetHardwareTimestampRate());
    metadata.hasTimestamp = (flags & SOAPY_SDR_HAS_TIME) != 0;
    metadata.endOfBurst = (flags & SOAPY_SDR_END_BURST) != 0;

    //write the 0th channel: get number of samples written
    int status = _conn->WriteStream(streamID[0], buffs[0], numElems, timeoutUs/1000, metadata);
    if (status == 0) return SOAPY_SDR_TIMEOUT;
    if (status < 0) return SOAPY_SDR_STREAM_ERROR;

    //write subsequent channels with the same size and large timeout
    //we should always be able to do a matching buffer write quickly
    //or there is an unknown internal issue with the stream fifo
    for (size_t i = 1; i < streamID.size(); i++)
    {
        int status_i = _conn->WriteStream(streamID[i], buffs[i], status, 1000/*1s*/, metadata);
        if (status_i != status)
        {
            SoapySDR::logf(SOAPY_SDR_ERROR, "Multi-channel stream alignment failed!");
            return SOAPY_SDR_CORRUPTION;
        }
    }

    //return num written
    return status;
}

int SoapyLMS7::readStreamStatus(
    SoapySDR::Stream *stream,
    size_t &chanMask,
    int &flags,
    long long &timeNs,
    const long timeoutUs)
{
    auto icstream = (IConnectionStream *)stream;
    const auto &streamID = icstream->streamID;

    StreamMetadata metadata;
    flags = 0;
    auto start = std::chrono::high_resolution_clock::now();
    while (1)
    {
        for(auto i : streamID)
        {
            int ret = _conn->ReadStreamStatus(i, timeoutUs/1000, metadata);
            if (ret != 0)
            {
                //handle the default not implemented case and return not supported
                if (GetLastError() == EPERM) return SOAPY_SDR_NOT_SUPPORTED;
                return SOAPY_SDR_TIMEOUT;
            }

            //packet dropped doesnt mean anything for tx streams
            if (icstream->direction == SOAPY_SDR_TX) metadata.packetDropped = false;

            //stop when event is detected
            if (metadata.endOfBurst || metadata.lateTimestamp || metadata.packetDropped)
                goto found;
        }
        //check timeout
        std::chrono::duration<double> seconds = std::chrono::high_resolution_clock::now()-start;
        if (seconds.count()> (double)timeoutUs/1e6)
            return SOAPY_SDR_TIMEOUT;
        //sleep to avoid high CPU load
        if (timeoutUs >= 2000)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        else
            std::this_thread::sleep_for(std::chrono::microseconds(1+timeoutUs/2));
    }

    found:
    timeNs = SoapySDR::ticksToTimeNs(metadata.timestamp, _conn->GetHardwareTimestampRate());
    //output metadata
    if (metadata.endOfBurst) flags |= SOAPY_SDR_END_BURST;
    if (metadata.hasTimestamp) flags |= SOAPY_SDR_HAS_TIME;

    if (metadata.lateTimestamp) return SOAPY_SDR_TIME_ERROR;
    if (metadata.packetDropped) return SOAPY_SDR_OVERFLOW;

    return 0;
}
