#include "osc/OscOutboundPacketStream.h"
#include "ip/UdpSocket.h"

#include <iostream>
#include <unistd.h>

#define ADDRESS "127.0.0.1"
#define PORT 6000

#define OUTPUT_BUFFER_SIZE 8192

int main()
{
    std::cout << "Communication OSC color demo" << std::endl;
    UdpTransmitSocket transmitSocket( IpEndpointName( ADDRESS, PORT ) );
    while (1)
    {
        char buffer[OUTPUT_BUFFER_SIZE];
        osc::OutboundPacketStream p(buffer, OUTPUT_BUFFER_SIZE );

        p << osc::BeginBundleImmediate;
        std::string messageName = "/colorLight";
        p << osc::BeginMessage(messageName.c_str());

        //////////// full RGB color example
        //        std::string tmp = "" + std::to_string(static_cast <float> (rand()) / (static_cast <float> (RAND_MAX/1.0)));
        //        tmp += " " + std::to_string(static_cast <float> (rand()) / (static_cast <float> (RAND_MAX/1.0)));
        //        tmp += " " + std::to_string(static_cast <float> (rand()) / (static_cast <float> (RAND_MAX/1.0)));
        //        p << tmp.c_str();

        p << static_cast <float> (rand()) / (static_cast <float> (RAND_MAX/1.0));

        p << osc::EndMessage;
        p << osc::EndBundle;
        transmitSocket.Send( p.Data(), p.Size() );

        usleep(10000);
    }

    return 0;
}

