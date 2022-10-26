#include "tello.hpp"

static std::vector<std::string> split(const std::string& target, char c) {
  std::string temp;
  std::stringstream stringstream{target};
  std::vector<std::string> result;

  while (std::getline(stringstream, temp, c)) {
    result.push_back(temp);
  }

  return result;
}

Tello::Tello() {
  commandSender_ = new SocketUdp;
  stateRecv_     = new SocketUdp;

  commandSender_->setIP((char*)IP_command);
  commandSender_->setPort(port_command);

  stateRecv_->setPort(port_state);

  commandSender_->bindServer();

  bool response = sendCommand("command");
  if (!response) {
    std::cout << "Error: Connecting to tello" << std::endl;
    exit(-1);  // FIXME
  }
  stateRecv_->bindServer();
  update();

  std::thread stateThd(&Tello::threadStateFnc, this);
  stateThd.detach();
  std::thread videoThd(&Tello::streamVideo, this);
  videoThd.detach();
}

Tello::~Tello() {
  commandSender_->~SocketUdp();
  stateRecv_->~SocketUdp();

  delete (commandSender_);
  delete (stateRecv_);
}

bool Tello::sendCommand(const std::string& command) {
  uint cont            = 0;
  const int timeLimit  = 10;
  std::string msgsBack = "";
  // std::cout << command << std::endl;
  do {
    commandSender_->sending(command);
    sleep(1);  // FIXME
    msgsBack = commandSender_->receiving();
    cont++;
    // cout << cont << endl;
  } while ((msgsBack.length() == 0) && (cont <= timeLimit));

  if (cont > timeLimit) {
    std::cout << "The command '" << command << "' is not received." << std::endl;
    return false;
  }
  return msgsBack == "ok";
}

void Tello::threadStateFnc() {
  bool resp;

  for (;;) {
    resp = getState();
    if (resp) update();
    sleep(0.2);  // FIXME
  }
}

bool Tello::getState() {
  std::string msgs = stateRecv_->receiving();

  if (msgs.length() == 0) {
    return false;
  }
  return parseState(msgs, state_);
}

bool Tello::parseState(const std::string& data, std::array<double, 16>& state) {
  std::vector<std::string> values, values_;
  values = split(data, ';');

  if (values.size() != state.size()) {
    std::cout << "Error: Adding data to the 'state' attribute" << std::endl;
    return false;
  }

  int i = 0;
  for (auto& value : values) {
    if (value.size()) {
      values_  = split(value, ':');
      state[i] = stod(values_[1]);
      i++;
    }
  }
  return true;
}

void Tello::update() {
  orientation_.x = state_[0];
  orientation_.y = state_[1];
  orientation_.z = state_[2];

  velocity_.x = state_[3];
  velocity_.y = state_[4];
  velocity_.z = state_[5];

  timeOF     = state_[8];
  height_    = state_[9];
  battery_   = (int)state_[10];
  timeMotor  = state_[12];
  barometer_ = state_[11];

  acceleration_.x = state_[13];
  acceleration_.y = state_[14];
  acceleration_.z = state_[15];

  imu_[0] = orientation_;
  imu_[1] = velocity_;
  imu_[2] = acceleration_;
}

void Tello::streamVideo() {
  bool response = sendCommand("streamon");

  if (response) {
    cv::VideoCapture capture{URL_stream, cv::CAP_FFMPEG};
    cv::Mat frame;

    while (true) {
      capture >> frame;
      if (!frame.empty()) {
        frame_ = frame;
      }
    }
  }
}

// Forward or backward move.
bool Tello::x_motion(double x) {
  bool response = true;
  std::string msg;
  if (x > 0) {
    msg      = "forward " + std::to_string(abs(x));
    response = sendCommand(msg);
  } else if (x < 0) {
    msg      = "back " + std::to_string(abs(x));
    response = sendCommand(msg);
  }
  return response;
}

// right or left move.
bool Tello::y_motion(double y) {
  bool response = true;
  std::string msg;
  if (y > 0) {
    msg      = "right " + std::to_string(abs(y));
    response = sendCommand(msg);
  } else if (y < 0) {
    msg      = "left " + std::to_string(abs(y));
    response = sendCommand(msg);
  }
  return response;
}

// up or left down.
bool Tello::z_motion(double z) {
  bool response = true;
  std::string msg;
  if (z > 0) {
    msg      = "up " + std::to_string(int(abs(z)));
    response = sendCommand(msg);
  } else if (z < 0) {
    msg      = "down " + std::to_string(int(abs(z)));
    response = sendCommand(msg);
  }
  return response;
}

// clockwise or counterclockwise
bool Tello::yaw_twist(double yaw) {
  bool response = true;
  std::string msg;
  if (yaw > 0) {
    msg      = "cw " + std::to_string(abs(yaw));
    response = sendCommand(msg);
  } else if (yaw < 0) {
    msg      = "ccw " + std::to_string(abs(yaw));
    response = sendCommand(msg);
  }
  return response;
}

// speed motion
bool Tello::speedMotion(double x, double y, double z, double yaw) {
  bool response;
  std::string msg;

  msg = "rc " + std::to_string(int(x)) + " " + std::to_string(int(y)) + " " +
        std::to_string(int(z)) + " " + std::to_string(int(yaw));
  response = sendCommand(msg);

  return response;
}