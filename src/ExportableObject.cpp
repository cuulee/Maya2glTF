#include "externals.h"
#include "Arguments.h"
#include "ExportableObject.h"
#include "ExportableResources.h"
#include "MayaException.h"

ExportableObject::ExportableObject(MObject mObj)
{
	MStatus status;
	MFnDependencyNode node(mObj, &status);
	THROW_ON_FAILURE(status);

	auto name = node.name(&status);
	THROW_ON_FAILURE(status);

	_name = name.asChar();
}

ExportableObject::~ExportableObject()
{
	
}

void ExportableObject::handleNameAssignment(const Arguments& args, GLTF::Object& glObj)
{
	if (args.assignObjectNames)
	{
		glObj.name = _name;
	}
}

void ExportableObject::handleNameAssignment(const ExportableResources& resources, GLTF::Object& glObj)
{
	handleNameAssignment(resources.arguments(), glObj);
}
