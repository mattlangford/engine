#pragma once

#include "synth/audio.hh"
#include "synth/node.hh"

namespace object::blocks {
class Speaker final : public synth::AbstractNode<1, 0> {
public:
    inline static const std::string kName = "Speaker";

public:
    Speaker(synth::AudioDriver& driver, size_t count) : AbstractNode{kName + std::to_string(count)}, driver_(driver) {}

public:
    void invoke(const Inputs& inputs, Outputs&) const override {
        driver_.write_inputs(inputs[0].samples.data(), inputs[0].samples.size());
    }

private:
    synth::AudioDriver& driver_;
};
}  // namespace object::blocks
