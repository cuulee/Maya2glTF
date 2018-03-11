#pragma once

#include "macros.h"
#include "accessors.h"

class ExportableNode;

class PropAnimation
{
public:
	PropAnimation(GLTF::Accessor& timesPerFrame, const GLTF::Node& node, const GLTF::Animation::Path path, const size_t dimension, const char* interpolation = "LINEAR")
		:dimension(dimension)
	{
		componentValuesPerFrame.reserve(timesPerFrame.count * dimension);

		glTarget.node = &const_cast<GLTF::Node&>(node);
		glTarget.path = path;

		glChannel.sampler = &glSampler;
		glChannel.target = &glTarget;

		glSampler.input = &timesPerFrame;
		glSampler.interpolation = interpolation;
	}

	~PropAnimation() = default;

	const size_t dimension;

	std::vector<float> componentValuesPerFrame;

	GLTF::Animation::Channel glChannel;
	GLTF::Animation::Sampler glSampler;
	GLTF::Animation::Channel::Target glTarget;

	template<std::ptrdiff_t Extent>
	void append(const gsl::span<const float, Extent>& components)
	{
		std::copy(components.begin(), components.end(), std::back_inserter(componentValuesPerFrame));
	}

	void finish()
	{
		if (!m_outputs)
		{
			// TODO: Allow passing name of node+path for debugging
			m_outputs = contiguousChannelAccessor(nullptr, span(componentValuesPerFrame), dimension);
		}

		glSampler.output = m_outputs.get();
	}

private:
	std::unique_ptr<GLTF::Accessor> m_outputs;

	DISALLOW_COPY_MOVE_ASSIGN(PropAnimation);
};
