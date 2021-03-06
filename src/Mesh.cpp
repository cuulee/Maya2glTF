#include "externals.h"
#include "Mesh.h"
#include "MayaException.h"
#include "Arguments.h"
#include "MayaUtils.h"
#include "MeshBlendShapeWeights.h"
#include "DagHelper.h"
#include "ExportableScene.h"

Mesh::Mesh(ExportableScene& scene, const MDagPath& dagPath)
{
	MStatus status;

	auto& args = scene.arguments();

	MFnMesh fnMesh(dagPath, &status);
	THROW_ON_FAILURE(status);

	MObject blendShapeDeformer = args.skipBlendShapes 
		? MObject::kNullObj 
		: tryExtractBlendShapeDeformer(fnMesh, args.ignoreMeshDeformers);

	if (blendShapeDeformer.isNull())
	{
		// Single shape
		m_mainShape = std::make_unique<MainShape>(scene, fnMesh, ShapeIndex::main());
		m_allShapes.emplace_back(m_mainShape.get());
	}
	else
	{
		// Shape with morph targets.
		MFnBlendShapeDeformer fnBlendShapeDeformer(blendShapeDeformer, &status);
		THROW_ON_FAILURE(status);

		const auto deformerName = fnBlendShapeDeformer.name().asChar();
		cout << prefix << "Processing blend shapes of " << deformerName << "..." << endl;

		const MPlug weightArrayPlug = fnBlendShapeDeformer.findPlug("weight", &status);
		THROW_ON_FAILURE(status);

		// We use the MeshBlendShapeWeights helper class to manipulate the weights 
		// in order to reconstruct the geometry of deleted blend shape targets.
		// When an exception is thrown, the weights and connections will be restored.
		MeshBlendShapeWeights weightPlugs(weightArrayPlug);
		weightPlugs.breakConnections();

		for (auto i=0; i<weightPlugs.numWeights(); ++i)
		{
			auto plugName = weightPlugs.getWeightPlug(i).name(&status);
			THROW_ON_FAILURE(status);
			cout << prefix << "target#" << i << ": " << std::quoted(plugName.asChar(), '\'') << endl;
			cout.flush();
		}

		// Clear all weights to reconstruct base mesh
		weightPlugs.clearWeightsExceptFor(-1);

		// Reconstruct base mesh
		m_mainShape = std::make_unique<MainShape>(scene, fnMesh, ShapeIndex::main());
		m_allShapes.emplace_back(m_mainShape.get());

		const auto numWeights = weightPlugs.numWeights();

		for (auto targetIndex = 0U; targetIndex < numWeights; targetIndex++)
		{
			weightPlugs.clearWeightsExceptFor(targetIndex);
			auto weightPlug = weightPlugs.getWeightPlug(targetIndex);
			auto initialWeight = static_cast<float>(weightPlugs.getOriginalWeight(targetIndex));
			auto blendShape = std::make_unique<MeshShape>(m_mainShape->indices(), 
				fnMesh, args, ShapeIndex::target(targetIndex), weightPlug, initialWeight);
			m_allShapes.emplace_back(blendShape.get());
			m_blendShapes.emplace_back(std::move(blendShape));
		}
	}
}

Mesh::~Mesh() = default;

MObject Mesh::tryExtractBlendShapeDeformer(const MFnMesh& fnMesh, const MSelectionList& ignoredDeformers)
{
	MObject deformer;

	// Iterate upstream to find all the nodes that affect the mesh.
	MStatus status;
	MPlug plug = fnMesh.findPlug("inMesh", status);
	THROW_ON_FAILURE(status);

	// TODO: Also look into inverted blend shapes through skinning, pose space deformations, etc..
	if (plug.isConnected())
	{
		MItDependencyGraph dgIt(plug,
			MFn::kInvalid,
			MItDependencyGraph::kUpstream,
			MItDependencyGraph::kBreadthFirst,
			MItDependencyGraph::kNodeLevel,
			&status);

		THROW_ON_FAILURE(status);

		dgIt.disablePruningOnFilter();

		for (; !dgIt.isDone(); dgIt.next())
		{
			MObject thisNode = dgIt.thisNode();
			if (thisNode.hasFn(MFn::kBlendShape))
			{
				MFnBlendShapeDeformer fnDeformer(thisNode, &status);

				const auto thisName = MFnDependencyNode(thisNode).name();

				if (status == MStatus::kSuccess)
				{
					if (ignoredDeformers.hasItem(thisNode))
					{
						cout << prefix << "ignoring blend shape deformer " << thisName << endl;
					}
					else if (deformer.isNull())
					{
						deformer = thisNode;
					}
					else
					{
						cerr << prefix << "only a single blend shape deformer is supported, skipping " << thisName << endl;
					}
				}
				else
				{
					cerr << prefix << "unable to extract blend deformer from " << thisName << ", reason: " << status.error() << endl;
				}
			}
		}
	}

	return deformer;
}

void Mesh::dump(class IndentableStream& out, const std::string& name) const
{
	out << quoted(name) << ": {" << endl << indent;

	JsonSeparator sep(",\n");

	for (auto& shape: m_allShapes)
	{
		out << sep;
		shape->dump(out, std::string("shape#") + std::to_string(shape->shapeIndex.arrayIndex()));
	}

	out << undent << '}';
}

Mesh::Cleanup::~Cleanup()
{
	if (!tempOutputMesh.isNull())
	{
		MDagModifier dagMod;
		dagMod.deleteNode(tempOutputMesh);
		tempOutputMesh = MObject::kNullObj;
	}
}

MObject Mesh::getOrCreateOutputShape(MPlug& outputGeometryPlug, MObject& createdMesh) const
{
	MStatus status;
	MPlugArray connections;
	outputGeometryPlug.connectedTo(connections, false, true, &status);
	THROW_ON_FAILURE(status);

	MObject meshNode = MObject::kNullObj;

	for (auto i = 0U; i < connections.length() && meshNode.isNull(); ++i)
	{
		MPlug targetPlug = connections[i];
		MObject targetNode = targetPlug.node();

		if (!targetNode.isNull())
		{
			if (targetNode.hasFn(MFn::kMesh))
			{
				meshNode = targetNode;
			}
		}
	}

	if (meshNode.isNull())
	{
		// Not connected to a mesh, create one.

		// Create the mesh node
		MDagModifier dagMod;
		createdMesh = dagMod.createNode("mesh", MObject::kNullObj, &status);
		THROW_ON_FAILURE(dagMod.doIt());

		// Make sure we select the shape node, not the transform node.
		MDagPath meshDagPath = MDagPath::getAPathTo(createdMesh, &status);
		THROW_ON_FAILURE(status);
		THROW_ON_FAILURE(meshDagPath.extendToShape());

		// NOTE: The node does not yet have an MFnMesh interface!!!
		// This only happens when it is connected to another node that delivers geometry...
		MFnDagNode dagFn(meshDagPath, &status);
		THROW_ON_FAILURE(status);

		const MString newSuggestedName = "maya2glTW_" + utils::simpleName(outputGeometryPlug.name(&status));
		THROW_ON_FAILURE(status);

		const MString newName = dagFn.setName(newSuggestedName, &status);
		THROW_ON_FAILURE(status);

		cout << prefix << "Created temporary output mesh '" << newName << "'. This will be deleted after exporting, but Maya will think your scene is modified, and warn you." << endl;

		// Make the mesh invisible
		MPlug intermediateObjectPlug = dagFn.findPlug("intermediateObject", &status);
		THROW_ON_FAILURE(status);
		THROW_ON_FAILURE(intermediateObjectPlug.setBool(true));

		MDGModifier dgMod;

		MPlug inMeshPlug = dagFn.findPlug("inMesh", &status);
		THROW_ON_FAILURE(status);

		THROW_ON_FAILURE(dgMod.connect(outputGeometryPlug, inMeshPlug));
		THROW_ON_FAILURE(dgMod.doIt());

		// Should now be a mesh...
		if (meshDagPath.hasFn(MFn::kMesh))
		{
			meshNode = meshDagPath.node(&status);
			THROW_ON_FAILURE(status);
		}
	}

	return meshNode;
}
