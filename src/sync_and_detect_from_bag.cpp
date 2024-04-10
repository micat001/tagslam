// -*-c++-*---------------------------------------------------------------------------------------
// Copyright 2024 Bernd Pfrommer <bernd.pfrommer@gmail.com>
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <rosbag2_transport/player.hpp>
#include <rosbag2_transport/recorder.hpp>
#include <tagslam/logging.hpp>
#include <tagslam/sync_and_detect.hpp>

static rclcpp::Logger get_logger()
{
  return (rclcpp::get_logger("sync_and_detect_from_bag"));
}

class MyPlayer : public rosbag2_transport::Player
{
public:
  MyPlayer(const std::string & name, const rclcpp::NodeOptions & opt)
  : rosbag2_transport::Player(name, opt)
  {
  }

  std::unordered_map<std::string, std::shared_ptr<rclcpp::GenericPublisher>>
  getPublishers()
  {
    return (rosbag2_transport::Player::get_publishers());
  }
};

static void printTopics(
  const std::string & label, const std::vector<std::string> & topics)
{
  for (const auto & t : topics) {
    LOG_INFO(label << ": " << t);
  }
}
static std::vector<std::string> merge(
  const std::vector<std::string> & v1, const std::vector<std::string> & v2)
{
  std::vector<std::string> all = v1;
  all.insert(all.end(), v2.begin(), v2.end());
  return (all);
}

static void checkThatTopicsAreInBag(
  const std::vector<std::string> & topics,
  const std::shared_ptr<MyPlayer> & player_node)
{
  const auto pubs = player_node->getPublishers();
  for (const auto & topic : topics) {
    if (pubs.find(topic) == pubs.end()) {
      LOG_ERROR("topic not in bag, sync will hang: " << topic);
    }
  }
}

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);

  rclcpp::executors::SingleThreadedExecutor exec;

  rclcpp::NodeOptions node_options;
  node_options.use_intra_process_comms(true);
  node_options.automatically_declare_parameters_from_overrides(true);

  auto sync_node = std::make_shared<tagslam::SyncAndDetect>(node_options);
  exec.add_node(sync_node);

  const auto in_topics =
    merge(sync_node->getImageTopics(), sync_node->getOdomTopics());
  printTopics("subscribed topic", in_topics);

  const auto recorded_topics =
    merge(sync_node->getTagTopics(), sync_node->getSyncedOdomTopics());
  printTopics("recorded topic", recorded_topics);

  const std::string in_uri =
    sync_node->get_parameter_or<std::string>("in_bag", "");
  if (in_uri.empty()) {
    LOG_ERROR("must provide valid in_bag parameter!");
    return (-1);
  }
  LOG_INFO("using input bag: " << in_uri);
  rclcpp::NodeOptions player_options;
  using Parameter = rclcpp::Parameter;
  player_options.parameter_overrides(
    {Parameter("storage.uri", in_uri), Parameter("play.topics", in_topics),
     Parameter("play.disable_keyboard_controls", true)});
  auto player_node =
    std::make_shared<MyPlayer>("rosbag_player", player_options);
  exec.add_node(player_node);

  checkThatTopicsAreInBag(in_topics, player_node);

  const std::string out_uri =
    sync_node->get_parameter_or<std::string>("out_bag", "");

  std::shared_ptr<rosbag2_transport::Recorder> recorder_node;
  if (!out_uri.empty()) {
    LOG_INFO("writing detected tags to bag: " << out_uri);
    rclcpp::NodeOptions recorder_options;
    recorder_options.parameter_overrides(
      {Parameter("storage.uri", out_uri),
       Parameter("record.disable_keyboard_controls", true),
       Parameter("record.topics", recorded_topics)});

    recorder_node = std::make_shared<rosbag2_transport::Recorder>(
      "rosbag_recorder", recorder_options);
    exec.add_node(recorder_node);
  }

  while (
    !player_node->wait_for_playback_to_finish(std::chrono::microseconds(0)) &&
    rclcpp::ok()) {
    exec.spin_some();
  }
  rclcpp::shutdown();
  return 0;
}
