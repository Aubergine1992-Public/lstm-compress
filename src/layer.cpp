#include "layer.h"

#include <math.h>
#include <algorithm>

namespace {
inline float Rand() {
  return static_cast <float> (rand()) / static_cast <float> (RAND_MAX);
}
inline float Logistic(float val) {
  return 1 / (1 + exp(-val));
}
}

Layer::Layer(unsigned int num_input, unsigned int num_cells, int horizon,
    float learning_rate) : state_(num_cells), hidden_(num_cells),
    hidden_error_(num_cells), output_gate_error_(num_cells),
    state_error_(num_cells), input_node_error_(num_cells),
    input_gate_error_(num_cells), forget_gate_error_(num_cells),
    stored_error_(num_cells),
    tanh_state_(std::valarray<float>(num_cells), horizon),
    output_gate_state_(std::valarray<float>(num_cells), horizon),
    input_node_state_(std::valarray<float>(num_cells), horizon),
    input_gate_state_(std::valarray<float>(num_cells), horizon),
    forget_gate_state_(std::valarray<float>(num_cells), horizon),
    last_state_(std::valarray<float>(num_cells), horizon),
    forget_gate_(std::valarray<float>(num_input), num_cells),
    input_node_(std::valarray<float>(num_input), num_cells),
    input_gate_(std::valarray<float>(num_input), num_cells),
    output_gate_(std::valarray<float>(num_input), num_cells),
    forget_gate_update_(std::valarray<float>(num_input), num_cells),
    input_node_update_(std::valarray<float>(num_input), num_cells),
    input_gate_update_(std::valarray<float>(num_input), num_cells),
    output_gate_update_(std::valarray<float>(num_input), num_cells),
    learning_rate_(learning_rate), num_cells_(num_cells), epoch_(0),
    horizon_(horizon) {
  float low = -0.2;
  float range = 0.4;
  for (unsigned int i = 0; i < forget_gate_.size(); ++i) {
    for (unsigned int j = 0; j < forget_gate_[i].size(); ++j) {
      forget_gate_[i][j] = low + Rand() * range;
      input_node_[i][j] = low + Rand() * range;
      input_gate_[i][j] = low + Rand() * range;
      output_gate_[i][j] = low + Rand() * range;
    }
    forget_gate_[i][forget_gate_[i].size() - 1] = 1;
  }
}

const std::valarray<float>& Layer::ForwardPass(const std::valarray<float>&
    input) {
  last_state_[epoch_] = state_;
  for (unsigned int i = 0; i < state_.size(); ++i) {
    forget_gate_state_[epoch_][i] = Logistic((input * forget_gate_[i]).sum());
    state_[i] *= forget_gate_state_[epoch_][i];
    input_node_state_[epoch_][i] = tanh((input * input_node_[i]).sum());
    input_gate_state_[epoch_][i] = Logistic((input * input_gate_[i]).sum());
    state_[i] += input_node_state_[epoch_][i] * input_gate_state_[epoch_][i];
    tanh_state_[epoch_][i] = tanh(state_[i]);
    output_gate_state_[epoch_][i] = Logistic((input * output_gate_[i]).sum());
    hidden_[i] = output_gate_state_[epoch_][i] * tanh_state_[epoch_][i];
  }
  ++epoch_;
  if (epoch_ == horizon_) epoch_ = 0;
  return hidden_;
}

const std::valarray<float>& Layer::BackwardPass(const std::valarray<float>&
    input, const std::valarray<float>& hidden_error, int epoch) {
  if (epoch < (int)horizon_ - 1) {
    stored_error_ += hidden_error;
  } else {
    stored_error_ = hidden_error;
  }

  output_gate_error_ = tanh_state_[epoch] * stored_error_ *
      output_gate_state_[epoch] * (1.0f - output_gate_state_[epoch]);
  state_error_ = stored_error_ * output_gate_state_[epoch] * (1.0f -
      (tanh_state_[epoch] * tanh_state_[epoch]));
  input_node_error_ = state_error_ * input_gate_state_[epoch] * (1.0f -
      (input_node_state_[epoch] * input_node_state_[epoch]));
  input_gate_error_ = state_error_ * input_node_state_[epoch] *
      input_gate_state_[epoch] * (1.0f - input_gate_state_[epoch]);
  forget_gate_error_ = state_error_ * last_state_[epoch] *
      forget_gate_state_[epoch] * (1.0f - forget_gate_state_[epoch]);

  hidden_error_ = 0;
  if (input.size() > 257 + num_cells_) {
    int offset = 256 + num_cells_;
    for (unsigned int i = 0; i < input_node_.size(); ++i) {
      for (unsigned int j = offset; j < input.size() - 1; ++j) {
        hidden_error_[j-offset] += input_node_[i][j] * input_node_error_[i];
        hidden_error_[j-offset] += input_gate_[i][j] * input_gate_error_[i];
        hidden_error_[j-offset] += forget_gate_[i][j] * forget_gate_error_[i];
        hidden_error_[j-offset] += output_gate_[i][j] * output_gate_error_[i];
      }
    }
  }

  if (epoch > 0) {
    stored_error_ = 0;
    for (unsigned int i = 0; i < input_node_.size(); ++i) {
      for (unsigned int j = 256; j < 256 + num_cells_; ++j) {
        stored_error_[j-256] += input_node_[i][j] * input_node_error_[i];
        stored_error_[j-256] += input_gate_[i][j] * input_gate_error_[i];
        stored_error_[j-256] += forget_gate_[i][j] * forget_gate_error_[i];
        stored_error_[j-256] += output_gate_[i][j] * output_gate_error_[i];
      }
    }
  }

  if (epoch == (int)horizon_ - 1) {
    for (unsigned int i = 0; i < input_node_.size(); ++i) {
      forget_gate_update_[i] = 0;
      input_node_update_[i] = 0;
      input_gate_update_[i] = 0;
      output_gate_update_[i] = 0;
    }
  }
  for (unsigned int i = 0; i < input_node_.size(); ++i) {
    forget_gate_update_[i] += (learning_rate_ * forget_gate_error_[i]) * input;
    input_node_update_[i] += (learning_rate_ * input_node_error_[i]) * input;
    input_gate_update_[i] += (learning_rate_ * input_gate_error_[i]) * input;
    output_gate_update_[i] += (learning_rate_ * output_gate_error_[i]) * input;
  }
  if (epoch == 0) {
    for (unsigned int i = 0; i < input_node_.size(); ++i) {
        forget_gate_[i] += forget_gate_update_[i];
        input_node_[i] += input_node_update_[i];
        input_gate_[i] += input_gate_update_[i];
        output_gate_[i] += output_gate_update_[i];
    }
  }
  return hidden_error_;
}
