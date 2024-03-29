/******************************************************************************
*       SOFA, Simulation Open-Framework Architecture, development version     *
*                (c) 2006-2017 INRIA, USTL, UJF, CNRS, MGH                    *
*                                                                             *
* This program is free software; you can redistribute it and/or modify it     *
* under the terms of the GNU Lesser General Public License as published by    *
* the Free Software Foundation; either version 2.1 of the License, or (at     *
* your option) any later version.                                             *
*                                                                             *
* This program is distributed in the hope that it will be useful, but WITHOUT *
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or       *
* FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License *
* for more details.                                                           *
*                                                                             *
* You should have received a copy of the GNU Lesser General Public License    *
* along with this program. If not, see <http://www.gnu.org/licenses/>.        *
*******************************************************************************
* Authors: The SOFA Team and external contributors (see Authors.txt)          *
*                                                                             *
* Contact information: contact@sofa-framework.org                             *
******************************************************************************/
#include <Communication/components/ServerCommunication.h>
#include <Communication/components/CommunicationSubscriber.h>

using sofa::core::RegisterObject ;

namespace sofa
{

namespace component
{

namespace communication
{

ServerCommunication::ServerCommunication()
    : d_job(initData(&d_job, OptionsGroup(2,"receiver","sender"), "job", "If unspecified, the default value is receiver"))
    , d_address(initData(&d_address, (std::string)"127.0.0.1", "address", "Connection address. (default=localhost)"))
    , d_port(initData(&d_port, (int)(6000), "port", "Port to listen (default=6000)"))
    , d_refreshRate(initData(&d_refreshRate, (double)(30.0), "refreshRate", "Refresh rate aka frequency (default=30), only used by sender"))
    , d_verbose(initData(&d_verbose, (bool)(false), "verbose", "Display debug messages (default=false)"))
{
}

ServerCommunication::~ServerCommunication()
{
    closeCommunication();
}

void ServerCommunication::init()
{
    f_listening = true;
    initTypeFactory();
	m_thread = std::thread(&ServerCommunication::thread_launcher, this);
}

void ServerCommunication::openCommunication()
{
    if (d_job.getValueString().compare("receiver") == 0)
    {
        receiveData();
    }
    else if (d_job.getValueString().compare("sender") == 0)
    {
        sendData();
    }
}

void ServerCommunication::closeCommunication()
{
    m_running = false;
    if(m_thread.joinable())
        m_thread.join();
}

void * ServerCommunication::thread_launcher(void *voidArgs)
{
    ServerCommunication *args = (ServerCommunication*)voidArgs;
    args->openCommunication();
    return NULL;
}

void ServerCommunication::handleEvent(Event * event)
{
    if (AnimateBeginEvent::checkEventType(event))
    {
        BufferData* data = fetchDatasFromReceivedBuffer();
        if (data == NULL) // simply check if the data is not null
            return;
        if(data->getRows() == -1 && data->getCols() == -1)
        {
            writeData(data);
            delete data;
            return;
        }

        if (!writeDataToFullMatrix(data))
            if (!writeDataToContainer(data))
                if(isVerbose())
                    msg_error() << "something went wrong while converting network data into sofa's data";
        delete data;
    }
    if (AnimateEndEvent::checkEventType(event))
    {
        saveDataToSenderBuffer();
    }
}

bool ServerCommunication::isVerbose()
{
    return d_verbose.getValue();
}

/******************************************************************************
*                                                                             *
* SUBSCRIBER PART                                                             *5
*                                                                             *
******************************************************************************/

bool ServerCommunication::isSubscribedTo(std::string subject, unsigned int argumentSize)
{
    try
    {
        CommunicationSubscriber* subscriber = m_subscriberMap.at(subject);
        if (subscriber->getArgumentSize() == argumentSize)
            return true;
        else
        {
            if(isVerbose())
                msg_warning(this->getName()) << " is subscribed to " << subject << " but datas should be size of " << subscriber->getArgumentSize() << ", received " << argumentSize << '\n';
        }
    } catch (const std::out_of_range& oor) {
        if(isVerbose())
            msg_warning(this->getName()) << " is not subscribed to " << subject << '\n';
    }
    return false;
}

CommunicationSubscriber * ServerCommunication::getSubscriberFor(std::string subject)
{
    try
    {
        return m_subscriberMap.at(subject);
    } catch (const std::out_of_range& oor) {
        if(isVerbose())
            msg_warning(this->getClassName()) << " is not subscribed to " << subject << '\n';
    }
    return nullptr;
}

void ServerCommunication::addSubscriber(CommunicationSubscriber * subscriber)
{
    m_subscriberMap.insert(std::pair<std::string, CommunicationSubscriber*>(subscriber->getSubject(), subscriber));
}

std::map<std::string, CommunicationSubscriber*> ServerCommunication::getSubscribers()
{
    return m_subscriberMap;
}

/******************************************************************************
*                                                                             *
* DATA PART                                                                   *
*                                                                             *
******************************************************************************/

BaseData* ServerCommunication::fetchData(SingleLink<CommunicationSubscriber, BaseObject, BaseLink::FLAG_DOUBLELINK> target, std::string keyTypeMessage, std::string argumentName)
{
    MapData dataMap = target->getDataAliases();
    MapData::const_iterator itData = dataMap.find(argumentName);
    BaseData* data;

    if (itData == dataMap.end())
    {
        data = getFactoryInstance()->createObject(keyTypeMessage, sofa::helper::NoArgument());
        if (data == nullptr)
        {
            if(isVerbose())
                msg_warning() << keyTypeMessage << " is not a known type";
        }
        else
        {
            data->setName(argumentName);
            data->setHelp("Auto generated help from communication");
            target->addData(data, argumentName);
            if(isVerbose())
                msg_info(target->getName()) << " data field named : " << argumentName << " of type " << keyTypeMessage << " has been created";
        }
    } else
        data = itData->second;
    return data;
}

bool ServerCommunication::writeData(BufferData* data)
{
    int i = 0;
    ArgumentList argumentList = data->getArgumentList();
    if (!isSubscribedTo(data->getSubject(), argumentList.size()))
        return false;
    CommunicationSubscriber * subscriber = getSubscriberFor(data->getSubject());
    if (!subscriber)
        return false;

    for (std::vector<std::string>::iterator it = argumentList.begin(); it != argumentList.end(); it++)
    {
        SingleLink<CommunicationSubscriber,  BaseObject, BaseLink::FLAG_DOUBLELINK> target = subscriber->getTarget();
        BaseData* baseData = fetchData(target, getArgumentType(*it),subscriber->getArgumentName(i));
        if (!baseData)
            continue;
        baseData->read(getArgumentValue(*it));
        i++;
    }
    return true;
}

bool ServerCommunication::writeDataToFullMatrix(BufferData* data)
{
    ArgumentList argumentList = data->getArgumentList();
    CommunicationSubscriber * subscriber = getSubscriberFor(data->getSubject());
    if (!subscriber)
        return false;
    std::string type = std::string("matrix") + getArgumentType(argumentList.at(0));
    SingleLink<CommunicationSubscriber,  BaseObject, BaseLink::FLAG_DOUBLELINK> target = subscriber->getTarget();
    BaseData* baseData = fetchData(target, type, subscriber->getArgumentName(0));
    std::string dataType = baseData->getValueTypeString();

    if(dataType.compare("FullMatrix<double>") == 0|| dataType.compare("FullMatrix<float>") == 0)
    {
        void* a = baseData->beginEditVoidPtr();
        FullMatrix<SReal> * b = static_cast<FullMatrix<SReal>*>(a);
        std::vector<std::string>::iterator it = argumentList.begin();
        b->resize(data->getRows(), data->getCols());
        for(int i = 0; i < b->rows(); i++)
        {
            for(int j = 0; j < b->cols(); j++)
            {
                b->set(i, j, stod(getArgumentValue(*it)));
                ++it;
            }
        }
        return true;
    }
    return false;
}

bool ServerCommunication::writeDataToContainer(BufferData* data)
{
    ArgumentList argumentList = data->getArgumentList();
    CommunicationSubscriber * subscriber = getSubscriberFor(data->getSubject());
    if (!subscriber)
        return false;
    std::string type = std::string("matrix") + getArgumentType(argumentList.at(0));
    SingleLink<CommunicationSubscriber,  BaseObject, BaseLink::FLAG_DOUBLELINK> target = subscriber->getTarget();
    BaseData* baseData = fetchData(target, type, subscriber->getArgumentName(0));
    const AbstractTypeInfo *typeinfo = baseData->getValueTypeInfo();

    if (!typeinfo->Container())
        return false;
    std::string value = "";
    for (std::vector<std::string>::iterator it = argumentList.begin(); it != argumentList.end(); it++)
    {
        value.append(" " + getArgumentValue(*it));
    }
    baseData->read(value);
    return true;
}



/******************************************************************************
*                                                                             *
* RECEIVED BUFFER PART                                                        *
*                                                                             *
******************************************************************************/

bool ServerCommunication::saveDatasToReceivedBuffer(std::string subject, ArgumentList argumentList, int rows, int cols)
{
    try
    {
        receiveDataBuffer->add(subject, argumentList, rows, cols);
    } catch (std::exception &exception) {
        if(isVerbose())
            msg_info("ServerCommunication") << exception.what();
        return false;
    }
    return true;
}

BufferData* ServerCommunication::fetchDatasFromReceivedBuffer()
{
    try
    {
        return receiveDataBuffer->get();
    } catch (std::exception &exception) {
        if(isVerbose())
            msg_info("ServerCommunication") << exception.what();
    }
    return NULL;
}

/******************************************************************************j
*                                                                             *
* SENDER BUFFER PART                                                          *
*                                                                             *
******************************************************************************/

bool ServerCommunication::saveDataToSenderBuffer()
{

    std::map<std::string, CommunicationSubscriber*> subscribersMap = getSubscribers();
    for (std::map<std::string, CommunicationSubscriber*>::iterator it = subscribersMap.begin(); it != subscribersMap.end(); it++)
    {
        CommunicationSubscriber* subscriber = it->second;
        ArgumentList argumentList = subscriber->getArgumentList();
        for (std::vector<std::string>::iterator itArgument = argumentList.begin(); itArgument != argumentList.end(); itArgument++ )
        {
            BaseData* data = fetchData(subscriber->getTarget(), defaultDataType(), *itArgument);
            if (!data)
                continue;
            std::string key = subscriber->getName() + *itArgument;
            std::map<std::string, CircularBufferSender*>::iterator it = senderDataMap.find(key);
            if(it == senderDataMap.end())
                senderDataMap.insert(std::pair<std::string, CircularBufferSender*>(key, new CircularBufferSender(this, 3)));
            CircularBufferSender * buffer = senderDataMap.at(key);
            try
            {
                buffer->add(data);
            } catch (std::exception &exception) {
                if(isVerbose())
                    msg_info("ServerCommunication") << exception.what();
            }
        }
    }
    return true;
}

BaseData* ServerCommunication::fetchDataFromSenderBuffer(CommunicationSubscriber* subscriber, std::string argument)
{
    std::string key = subscriber->getName() + argument;
    std::map<std::string, CircularBufferSender*>::iterator it = senderDataMap.find(key);
    if(it == senderDataMap.end())
        return NULL;;
    CircularBufferSender * buffer = senderDataMap.at(key);

    try
    {
        return buffer->get();
    } catch (std::exception &exception) {
        if(isVerbose())
            msg_info("ServerCommunication") << exception.what();
    }
    return NULL;

}

} /// communication

} /// component

} /// sofa
