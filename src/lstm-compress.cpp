#include "lstm-compress.h"

namespace {
inline float Rand() {
  return static_cast <float> (rand()) / static_cast <float> (RAND_MAX);
}
inline float Logistic(float val) {
  return 1 / (1 + exp(-val));
}
}

LstmCompress::LstmCompress(unsigned int num_cells, unsigned int num_layers,
    int horizon, float learning_rate) : input_history_(horizon),
    probs_(1.0 / 256, 256), hidden_(num_cells * num_layers + 1),
    hidden_error_(num_cells),
    layer_input_(std::valarray<std::valarray<float>>(std::valarray<float>
    (257 + num_cells * 2), num_layers), horizon),
    output_layer_(std::valarray<std::valarray<float>>(std::valarray<float>
    (num_cells * num_layers + 1), 256), horizon),
    output_(std::valarray<float>(1.0 / 256, 256), horizon),
    learning_rate_(learning_rate), num_cells_(num_cells), epoch_(0),
    horizon_(horizon) {
  hidden_[hidden_.size() - 1] = 1;
  for (int epoch = 0; epoch < horizon; ++epoch) {
    layer_input_[epoch][0].resize(257 + num_cells);
    for (unsigned int i = 0; i < num_layers; ++i) {
      layer_input_[epoch][i][layer_input_[epoch][i].size() - 1] = 1;
    }
  }
  for (unsigned int i = 0; i < num_layers; ++i) {
    layers_.push_back(std::unique_ptr<Layer>(new Layer(layer_input_[0][i].
        size(), num_cells, horizon, learning_rate)));
  }
  float low = -0.2;
  float range = 0.4;
  for (unsigned int i = 0; i < output_layer_[0].size(); ++i) {
    for (unsigned int j = 0; j < output_layer_[0][i].size(); ++j) {
      output_layer_[0][i][j] = low + Rand() * range;
    }
  }
}

std::valarray<float>& LstmCompress::Perceive(unsigned char input) {
  int last_epoch = epoch_ - 1;
  if (last_epoch == -1) last_epoch = horizon_ - 1;
  input_history_[last_epoch] = input;
  if (epoch_ == 0) {
    for (int epoch = horizon_ - 1; epoch >= 0; --epoch) {
      for (int layer = layers_.size() - 1; layer >= 0; --layer) {
        int offset = layer * num_cells_;
        for (unsigned int i = 0; i < 256; ++i) {
          float error = 0;
          if (i == input_history_[epoch]) error = (1 - output_[epoch][i]);
          else error = -output_[epoch][i];
          for (unsigned int j = 0; j < hidden_error_.size(); ++j) {
            hidden_error_[j] += output_layer_[epoch][i][j + offset] * error;
          }
        }
        hidden_error_ = layers_[layer]->BackwardPass(layer_input_[epoch][layer],
            hidden_error_, epoch);
      }
    }
  }

  output_layer_[epoch_] = output_layer_[last_epoch];
  for (unsigned int i = 0; i < 256; ++i) {
    float error = 0;
    if (i == input) error = (1 - output_[last_epoch][i]);
    else error = -output_[last_epoch][i];
    output_layer_[epoch_][i] += learning_rate_ * error * hidden_;
  }
  return Predict(input);
}

std::valarray<float>& LstmCompress::Predict(unsigned char input) {
  for (unsigned int i = 0; i < layers_.size(); ++i) {
    std::fill_n(begin(layer_input_[epoch_][i]), 256, 0);
    layer_input_[epoch_][i][input] = 1;
    auto start = begin(hidden_) + i * num_cells_;
    std::copy(start, start + num_cells_, begin(layer_input_[epoch_][i]) + 256);
    const auto& hidden = layers_[i]->ForwardPass(layer_input_[epoch_][i]);
    std::copy(begin(hidden), end(hidden), start);
    if (i < layers_.size() - 1) {
      start = begin(layer_input_[epoch_][i + 1]) + 256 + num_cells_;
      std::copy(begin(hidden), end(hidden), start);
    }
  }
  for (unsigned int i = 0; i < 256; ++i) {
    output_[epoch_][i] = Logistic((hidden_ * output_layer_[epoch_][i]).sum());
  }
  probs_ = output_[epoch_];
  double sum = 0, min = 0.000001;
  for (int i = 0; i < 256; ++i) {
    if (probs_[i] < min) probs_[i] = min;
    sum += probs_[i];
  }
  probs_ /= sum;
  ++epoch_;
  if (epoch_ == horizon_) epoch_ = 0;
  return probs_;
}
