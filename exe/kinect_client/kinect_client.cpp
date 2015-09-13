#include <iostream>
#include <string>
#include <sstream>
#include <thread>
#include <chrono>

#include <Poco/Net/SocketAddress.h>
#include <Poco/Net/StreamSocket.h>
#include <Poco/Net/SocketStream.h>

#include <atomics/binary_stream.h>

#include <messages/kinect_messages.h>
#include <messages/binary_codec.h>
#include <messages/message_coder.h>

#include <messages/input_tcp_device.h>

#include <ros/ros.h>

#include <kinect_bridge2/KinectSpeech.h>
#include <kinect_bridge2/KinectBodies.h>

class KinectBridge2Client
{
public:
    typedef kinect_bridge2::KinectSpeech _KinectSpeechMsg;
    typedef kinect_bridge2::KinectSpeechPhrase _KinectSpeechPhraseMsg;

    typedef kinect_bridge2::KinectBodies _KinectBodiesMsg;
    typedef kinect_bridge2::KinectBody _KinectBodyMsg;
    typedef kinect_bridge2::KinectJoint _KinectJointMsg;

    ros::NodeHandle nh_rel_;

    ros::Publisher kinect_speech_pub_;
    ros::Publisher kinect_bodies_pub_;

    InputTCPDevice kinect_bridge_client_;

    MessageCoder<BinaryCodec<> > binary_message_coder_;

    KinectBridge2Client( ros::NodeHandle & nh_rel )
    :
        nh_rel_( nh_rel ),
        kinect_speech_pub_( nh_rel_.advertise<_KinectSpeechMsg>( "speech", 10 ) ),
        kinect_bodies_pub_( nh_rel_.advertise<_KinectBodiesMsg>( "bodies", 10 ) ),
        kinect_bridge_client_( "localhost", 5903 )
    {
        //
    }

    void spin()
    {
        while( ros::ok() && kinect_bridge_client_.input_socket_.impl()->initialized() )
        {
            while( ros::ok() && !kinect_bridge_client_.input_socket_.available() )
            {
                std::cout << "waiting for content..." << std::endl;
                std::this_thread::sleep_for( std::chrono::milliseconds( 100 ) );
                ros::spinOnce();
            }

            if( !ros::ok() ) return;

            std::cout << "got message" << std::endl;
            CodedMessage<> binary_coded_message;
            kinect_bridge_client_.pull( binary_coded_message );
            processKinectMessage( binary_coded_message );

            ros::spinOnce();
        }
    }

    void processKinectMessage( CodedMessage<> & coded_message )
    {
        auto & coded_header = coded_message.header_;
        std::cout << "processing message type: " << coded_message.header_.payload_type_ << std::endl;

        if( coded_header.payload_type_ == "KinectSpeechMessage" )
        {
            auto kinect_speech_message = binary_message_coder_.decode<KinectSpeechMessage>( coded_message );

            auto & header = kinect_speech_message.header_;
            auto & payload = kinect_speech_message.payload_;

            _KinectSpeechMsg ros_kinect_speech_message;

            for( size_t i = 0; i < payload.size(); ++i )
            {
                _KinectSpeechPhraseMsg ros_kinect_speech_phrase_message;
                ros_kinect_speech_phrase_message.tag = payload[i].tag_;
                ros_kinect_speech_phrase_message.confidence = payload[i].confidence_;
                ros_kinect_speech_message.phrases.emplace_back( std::move( ros_kinect_speech_phrase_message ) );
            }

            kinect_speech_pub_.publish( ros_kinect_speech_message );
        }
        else if( coded_header.payload_type_ == "KinectBodiesMessage" )
        {
            auto bodies_msg = binary_message_coder_.decode<KinectBodiesMessage>( coded_message );

            auto const & header = bodies_msg.header_;
            auto const & payload = bodies_msg.payload_;

            _KinectBodiesMsg ros_bodies_msg;

            for( size_t body_idx = 0; body_idx < payload.size(); ++body_idx )
            {
                _KinectBodyMsg ros_body_msg;
                auto const & body_msg = payload[body_idx];

                ros_body_msg.is_tracked = body_msg.is_tracked_;
                ros_body_msg.hand_state_left = static_cast<uint8_t>( body_msg.hand_state_left_ );
                ros_body_msg.hand_state_right = static_cast<uint8_t>( body_msg.hand_state_right_ );

                auto const & joints_msg = body_msg.joints_;
                auto & ros_joints_msg = ros_body_msg.joints;

                for( size_t joint_idx = 0; joint_idx < joints_msg.size(); ++joint_idx )
                {
                    auto const & joint_msg = joints_msg[joint_idx];
                    _KinectJointMsg ros_joint_msg;

                    ros_joint_msg.joint_type = static_cast<uint8_t>( joint_msg.joint_type_ );
                    ros_joint_msg.tracking_state = static_cast<uint8_t>( joint_msg.tracking_state_ );

                    ros_joint_msg.position.x = joint_msg.position_.x;
                    ros_joint_msg.position.y = joint_msg.position_.y;
                    ros_joint_msg.position.z = joint_msg.position_.x;

                    ros_joint_msg.orientation.x = joint_msg.orientation_.x;
                    ros_joint_msg.orientation.y = joint_msg.orientation_.y;
                    ros_joint_msg.orientation.z = joint_msg.orientation_.x;
                    ros_joint_msg.orientation.w = joint_msg.orientation_.w;

                    ros_joints_msg.emplace_back( std::move( ros_joint_msg ) );
                }
                ros_bodies_msg.bodies.emplace_back( std::move( ros_body_msg ) );
            }
            kinect_bodies_pub_.publish( ros_bodies_msg );

            // TODO: TF stuff here
        }
    }
};

int main( int argc, char ** argv )
{
    ros::init( argc, argv, "kinect_client" );
    ros::NodeHandle nh_rel( "~" );

    KinectBridge2Client kinect_bridge2_client( nh_rel );

    kinect_bridge2_client.spin();

    return 0;
}
