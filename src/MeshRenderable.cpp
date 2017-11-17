#include "externals.h"
#include "MeshRenderable.h"
#include "MeshIndices.h"
#include "MeshVertices.h"
#include "dump.h"
#include "MayaException.h"
#include "DagHelper.h"
#include "MeshShape.h"

MeshRenderable::MeshRenderable(
	const int meshInstanceIndex,
	const int meshShaderIndex, 
	const MeshShape& shape)
	: meshInstanceIndex(meshInstanceIndex)
	, meshShaderIndex(meshShaderIndex)
{
	MStatus status;

	const auto& meshIndices = shape.indices();
	const auto& meshVertices = shape.vertices();
	const auto& meshSemantics = shape.semantics();

	const MeshShading& shading = meshIndices.shadingPerInstance().at(meshInstanceIndex);

	m_shaderGroup = shading.shaderGroups[meshShaderIndex];

	std::map<IndexVector, Index> drawableComponentIndexMap;

	const auto& shaderMap = shading.primitiveToShaderIndexMap;
	const auto& indicesTable = meshIndices.table();
	const auto& componentsTable = meshVertices.table();

	const auto primitiveCount = shaderMap.size();

	const auto keySize = meshSemantics.totalSetCount();

	IndexVector key;
	key.reserve(keySize);

	int lastVertexIndex = -1;

	uint64 vertexMask = 0;

	// Allocate drawable component vectors
	for (auto semanticIndex = 0; semanticIndex < Semantic::COUNT; ++semanticIndex)
	{
		const auto semanticKind = Semantic::from(semanticIndex);
		const auto& sourceComponentsPerSet = componentsTable.at(semanticKind);
		auto& targetComponentsPerSet = m_table.at(semanticKind);

		for (auto setIndex = 0; setIndex < sourceComponentsPerSet.size(); ++setIndex)
		{
			const auto& source = sourceComponentsPerSet[setIndex];

			FloatVector target;
			target.reserve(source.size() * 2);
			targetComponentsPerSet.push_back(target);
		}
	}

	// Merge components
	int componentIndex = 0;
	const int componentsPerPrimitive = 3;

	for (int primitiveIndex = 0; primitiveIndex < primitiveCount; ++primitiveIndex)
	{
		if (shaderMap[primitiveIndex] != meshShaderIndex)
		{
			// This primitive does not belong to the shader, skip it.
			componentIndex += componentsPerPrimitive;
			continue;
		}

		// Add each component of the primitive.
		for (int counter = componentsPerPrimitive; --counter >= 0; ++componentIndex)
		{
			key.clear();

			uint64 keyMask = 0;

			for (auto && indicesPerSet : indicesTable)
			{
				for (auto && indices : indicesPerSet)
				{
					const auto index = indices[componentIndex];
					key.push_back(index);

					keyMask <<= 1;
					keyMask |= index >= 0;
				}
			}

			if (vertexMask && vertexMask != keyMask)
				throw std::exception("Mesh seems to have invalid set assignments");

			vertexMask = keyMask;

			int vertexIndex;

			const auto foundIt = drawableComponentIndexMap.find(key);

			if (foundIt != drawableComponentIndexMap.end())
			{
				// Reuse the vertex
				vertexIndex = foundIt->second;
			}
			else
			{
				// Create a new vertex
				vertexIndex = ++lastVertexIndex;

				drawableComponentIndexMap[key] = vertexIndex;

				auto keyIndex = 0;
				for (auto semanticIndex = 0; semanticIndex < Semantic::COUNT; ++semanticIndex)
				{
					const auto semanticKind = Semantic::from(semanticIndex);
					const auto groupSize = Semantic::dimension(semanticKind);
					const auto& sourceComponentsPerSet = componentsTable.at(semanticKind);
					auto& targetComponentsPerSet = m_table.at(semanticKind);

					for (auto setIndex = 0; setIndex < sourceComponentsPerSet.size(); ++setIndex)
					{
						const auto& source = sourceComponentsPerSet[setIndex];
						auto& target = targetComponentsPerSet[setIndex];

						const auto sourceIndex = key[keyIndex++];

						if (sourceIndex >= 0)
						{
							target.insert(target.end(),
								source.begin() + groupSize * sourceIndex,
								source.begin() + groupSize * (sourceIndex + 1));
						}
					}
				}
			}


			m_indices.push_back(vertexIndex);
		}
	}

}


MeshRenderable::~MeshRenderable()
{
}

void MeshRenderable::dump(const std::string& indent) const
{
	const auto subIndent = indent + "\t";
	cout << indent << "{" << endl;
	cout << subIndent << std::quoted("meshShaderIndex") << ":" << std::to_string(meshShaderIndex) << "," << endl;
	dump_table("vertices", m_table, subIndent);
	cout << "," << endl;
	dump_iterable("indices", m_indices, subIndent);
	cout << "," << endl;
	cout << indent << "}";
}

