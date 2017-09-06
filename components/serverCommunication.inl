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
#include <Communication/components/serverCommunication.h>
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
    , d_address(initData(&d_address, (std::string)"127.0.0.1", "address", "Scale for object display. (default=localhost)"))
    , d_port(initData(&d_port, (int)(6000), "port", "Port to listen (default=6000)"))
    , d_refreshRate(initData(&d_refreshRate, (double)(30.0), "refreshRate", "Refres rate aka frequency (default=30), only used by sender"))
{
    //    pthread_mutex_init(&mutex, NULL);
}

ServerCommunication::~ServerCommunication()
{
    closeCommunication();
}

void ServerCommunication::init()
{
    f_listening = true;
    initTypeFactory();
    pthread_create(&m_thread, NULL, &ServerCommunication::thread_launcher, this);
}

void ServerCommunication::handleEvent(Event * event)
{
    //    if (sofa::simulation::AnimateBeginEvent::checkEventType(event) && d_job.getValueString().compare("sender") == 0)
    //    {
    //        pthread_mutex_lock(&mutex);
    //        for( size_t i=0 ; i < this->d_data.size(); ++i )
    //        {
    //            this->d_data_copy[i] = this->d_data[i];
    //        }
    //        pthread_mutex_unlock(&mutex);

    //#if BENCHMARK
    //        // Uncorrect results if frequency == 1hz, due to tv_usec precision
    //        gettimeofday(&t1, NULL);
    //        if(d_refreshRate.getValue() <= 1.0)
    //            std::cout << "Animation Loop frequency : " << fabs((t1.tv_sec - t2.tv_sec)) << " s or " << fabs(1.0 / ((t1.tv_sec - t2.tv_sec))) << " hz"<< std::endl;
    //        else
    //            std::cout << "Animation Loop frequency : " << fabs((t1.tv_usec - t2.tv_usec) / 1000.0) << " ms or " << fabs(1000000.0 / ((t1.tv_usec - t2.tv_usec))) << " hz"<< std::endl;
    //        gettimeofday(&t2, NULL);
    //#endif
    //    }
    //    else if (sofa::simulation::AnimateEndEvent::checkEventType(event) && d_job.getValueString().compare("receiver") == 0)
    //    {
    //        pthread_mutex_lock(&mutex);
    //        for( size_t i=0 ; i < this->d_data_copy.size(); ++i )
    //        {
    //            this->d_data[i] = this->d_data_copy[i];
    //        }
    //        pthread_mutex_unlock(&mutex);

    //#if BENCHMARK
    //        // Uncorrect results if frequency == 1hz, due to tv_usec precision
    //        gettimeofday(&t1, NULL);
    //        if(d_refreshRate.getValue() <= 1.0)
    //            std::cout << "Animation Loop frequency : " << fabs((t1.tv_sec - t2.tv_sec)) << " s or " << fabs(1.0 / ((t1.tv_sec - t2.tv_sec))) << " hz"<< std::endl;
    //        else
    //            std::cout << "Animation Loop frequency : " << fabs((t1.tv_usec - t2.tv_usec) / 1000.0) << " ms or " << fabs(1000000.0 / ((t1.tv_usec - t2.tv_usec))) << " hz"<< std::endl;
    //        gettimeofday(&t2, NULL);
    //#endif
    //    }
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
    pthread_join(m_thread, NULL);
}

void * ServerCommunication::thread_launcher(void *voidArgs)
{
    ServerCommunication *args = (ServerCommunication*)voidArgs;
    args->openCommunication();
    return NULL;
}

bool ServerCommunication::isSubscribedTo(std::string subject, unsigned int argumentSize)
{
    try
    {
        CommunicationSubscriber* subscriber = m_subscriberMap.at(subject);
        if (subscriber->getArgumentSize() == argumentSize)
            return true;
        else
        {
            msg_warning(this->getName()) << " is subscrided to " << subject << " but arguments should be size of " << subscriber->getArgumentSize() << ", received " << argumentSize << '\n';
        }
    } catch (const std::out_of_range& oor) {
        msg_warning(this->getName()) << " is not subscrided to " << subject << '\n';
    }
    return false;
}

CommunicationSubscriber * ServerCommunication::getSubscriberFor(std::string subject)
{
    try
    {
        return m_subscriberMap.at(subject);
    } catch (const std::out_of_range& oor) {
        msg_warning(this->getClassName()) << " is not subscrided to " << subject << '\n';
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


} /// communication

} /// component

} /// sofa