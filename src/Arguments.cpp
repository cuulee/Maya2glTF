#include "externals.h"
#include "Arguments.h"
#include "MayaException.h"
#include "IndentableStream.h"

using namespace std::experimental;

namespace flag
{
	const auto outputFolder = "of";
	const auto sceneName = "sn";
	const auto glb = "glb";
	const auto dumpMaya = "dmy";
	const auto dumpGLTF = "dgl";
	const auto separate = "sep";
	const auto defaultMaterial = "dm";
	const auto colorizeMaterials = "cm";
	const auto forcePbrMaterials = "fpm";
	const auto force32bitIndices = "i32";
	const auto assignObjectNames = "aon";
	const auto scaleFactor = "sf";
	const auto mikkelsenTangentSpace = "mts";
	const auto mikkelsenTangentAngularThreshold = "mta";
	const auto globalOpacityFactor = "gof";

	const auto animationClipNames = "acn";
	const auto animationClipFrameRate = "afr";
	const auto animationClipStartTime = "ast";
	const auto animationClipEndTime = "aet";

	const auto initialValuesTime = "ivt";

	const auto redrawViewport = "rvp";

	const auto debugTangentVectors = "dtv";
	const auto debugNormalVectors = "dnv";
	const auto debugVectorLength = "dvl";

	const auto meshPrimitiveAttributes = "mpa";

	const auto skipSkinClusters = "ssc";
	const auto skipBlendShapes = "sbs";
	const auto ignoreMeshDeformers = "imd";
}

inline const char* getArgTypeName(const MSyntax::MArgType argType)
{
	switch (argType)
	{
	case MSyntax::kNoArg:
		return "flag";
	case MSyntax::kBoolean:
		return "boolean";
	case MSyntax::kLong:
		return "integer";
	case MSyntax::kDouble:
		return "float";
	case MSyntax::kString:
		return "string";
	case MSyntax::kUnsigned:
		return "unsigned";
	case MSyntax::kDistance:
		return "distance";
	case MSyntax::kAngle:;
		return "angle";
	case MSyntax::kTime:
		return "time";
	case MSyntax::kSelectionItem:
		return "selectionItem";
	default:
		return nullptr;
	}
}

SyntaxFactory::SyntaxFactory()
{
	auto status = setObjectType(kSelectionList, 1);
	THROW_ON_FAILURE(status);

	useSelectionAsDefault(true);

	std::stringstream ss;
	registerFlag(ss, flag::outputFolder, "outputFolder", kString);
	registerFlag(ss, flag::sceneName, "sceneName", kString);
	registerFlag(ss, flag::scaleFactor, "scaleFactor", kDouble);
	registerFlag(ss, flag::glb, "binary", kNoArg);
	registerFlag(ss, flag::dumpGLTF, "dumpGTLF", kString);
	registerFlag(ss, flag::dumpMaya, "dumpMaya", kString);
	registerFlag(ss, flag::separate, "separate", kNoArg);
	registerFlag(ss, flag::defaultMaterial, "defaultMaterial", kNoArg);
	registerFlag(ss, flag::colorizeMaterials, "colorizeMaterials", kNoArg);
	registerFlag(ss, flag::forcePbrMaterials, "forcePbrMaterials", kNoArg);
	registerFlag(ss, flag::force32bitIndices, "force32bitIndices", kNoArg);
	registerFlag(ss, flag::assignObjectNames, "assignObjectNames", kNoArg);
	registerFlag(ss, flag::mikkelsenTangentSpace, "mikkelsenTangentSpace", kNoArg);
	registerFlag(ss, flag::mikkelsenTangentAngularThreshold, "mikkelsenTangentAngularThreshold", kDouble);
	registerFlag(ss, flag::debugNormalVectors, "debugNormalVectors", kNoArg);
	registerFlag(ss, flag::debugTangentVectors, "debugTangentVectors", kNoArg);
	registerFlag(ss, flag::debugVectorLength, "debugVectorLength", kDouble);
	registerFlag(ss, flag::globalOpacityFactor, "globalOpacityFactor", kDouble);

	registerFlag(ss, flag::animationClipFrameRate, "animationClipFrameRate", true, kDouble);
	registerFlag(ss, flag::animationClipNames, "animationClipNames", true, kString);
	registerFlag(ss, flag::animationClipStartTime, "animationClipStartTime", true, kTime);
	registerFlag(ss, flag::animationClipEndTime, "animationClipEndTime", true, kTime);

	registerFlag(ss, flag::initialValuesTime, "initialValuesTime", kTime);

	registerFlag(ss, flag::meshPrimitiveAttributes, "meshPrimitiveAttributes", kString);

	registerFlag(ss, flag::ignoreMeshDeformers, "ignoreMeshDeformers", true, kString);
	registerFlag(ss, flag::skipSkinClusters, "skipSkinClusters", kNoArg);
	registerFlag(ss, flag::skipBlendShapes, "skipBlendShapes", kNoArg);

	m_usage = ss.str();
}

SyntaxFactory::~SyntaxFactory()
{
}

const SyntaxFactory& SyntaxFactory::get()
{
	static SyntaxFactory syntax;
	return syntax;
}

MSyntax SyntaxFactory::createSyntax()
{
	return static_cast<MSyntax>(get());
}

void SyntaxFactory::registerFlag(std::stringstream& ss, const char* shortName, const char* longName, const MArgType argType1)
{
	registerFlag(ss, shortName, longName, false, argType1);
}

void SyntaxFactory::registerFlag(std::stringstream& ss, const char *shortName, const char *longName, const bool isMultiUse, const MArgType argType1)
{
	m_argNames[shortName] = longName;

	auto status = addFlag(shortName, longName, argType1);
	THROW_ON_FAILURE(status);

	if (isMultiUse)
	{
		makeFlagMultiUse(shortName);
	}

	const auto name1 = getArgTypeName(argType1);

	ss << "-" << std::setw(5) << std::left << shortName << longName;

	if (name1)
	{
		ss << ": " << name1;
	}

	if (isMultiUse)
	{
		ss << "+";
	}

	ss << endl;

	ss.flush();
}

class ArgChecker
{
public:
	ArgChecker(const MSyntax &syntax, const MArgList &argList, MStatus& status)
		:adb(syntax, argList, &status)
	{
		// TODO: How to provide more error information about what arguments are wrong?
		throwOnFailure(status, "Invalid arguments");
	}

	void getObjects(MSelectionList& selection) const
	{
		throwOnFailure(adb.getObjects(selection), "failed to get selection");

		if (selection.length() < 1)
			throwOnFailure(MStatus::kInvalidParameter, "At least one object must be selected or passed to the command");
	}

	bool isFlagSet(const char* shortName) const
	{
		MStatus status;
		const auto result = adb.isFlagSet(shortName, &status);
		throwOnArgument(status, shortName);
		return result;
	}

	int flagUsageCount(const char* shortName) const
	{
		return adb.numberOfFlagUses(shortName);
	}

	template<typename T>
	void required(const char* shortName, T& value, const int index = 0) const
	{
		if (!isFlagSet(shortName))
			throwInvalid(shortName, "Missing argument");

		const auto status = adb.getFlagArgument(shortName, index, value);
		throwOnArgument(status, shortName);
	}

	template<typename T>
	bool optional(const char* shortName, T& value, const int index = 0) const
	{
		if (!adb.isFlagSet(shortName))
			return false;

		const auto status = adb.getFlagArgument(shortName, index, value);
		throwOnArgument(status, shortName);
		return true;
	}

	bool optional(const char* shortName, float& value, const int index = 0) const
	{
		double temp;
		if (!optional(shortName, temp, index))
			return false;
		value = static_cast<float>(temp);
		return true;
	}

	std::unique_ptr<IndentableStream> getOutputStream(const char* arg, const char *outputName, const path& outputFolder, std::ofstream& fileOutputStream) const
	{
		std::ostream* out = nullptr;

		if (adb.isFlagSet(arg))
		{
			MString argPath;
			if (adb.getFlagArgument(arg, 0, argPath).error() || argPath.toLowerCase() == "console")
			{
				out = &cout;
			}
			else if (argPath.length() == 0 || argPath.substring(0, 0) == "-")
			{
				throwInvalid(arg, "requires an output filepath argument, or just 'console' to print to Maya's console window");
			}
			else
			{
				const path argumentPath(argPath.asChar());
				const path absolutePath = argumentPath.is_relative()
					? outputFolder / argumentPath
					: argumentPath;

				cout << prefix << "Writing " << outputName << " output to file " << absolutePath << endl;

				fileOutputStream.open(absolutePath);
				out = &fileOutputStream;
			}
		}

		return out ? std::make_unique<IndentableStream>(*out) : nullptr;
	}

	static void throwOnFailure(const MStatus& status, const char* message)
	{
		if (status.error())
		{
			const auto statusStr = status.errorString().asChar();
			const auto usageStr = SyntaxFactory::get().usage();
			throw MayaException(status,
				formatted("%s (%s)\nUsage:\n%s", message, statusStr, usageStr));
		}
	}

	static void throwUsage(const char* message)
	{
		const auto usageStr = SyntaxFactory::get().usage();
		throw MayaException(MStatus::kFailure,
			formatted("%s\nUsage:\n%s", message, usageStr));
	}

	static void throwOnArgument(const MStatus& status, const char* shortArgName)
	{
		if (status.error())
		{
			const auto longArgName = SyntaxFactory::get().longArgName(shortArgName);
			const auto statusStr = status.errorString().asChar();
			const auto usageStr = SyntaxFactory::get().usage();
			throw MayaException(status,
				formatted("-%s (%s): %s\nUsage:\n%s", shortArgName, longArgName, statusStr, usageStr));
		}
	}

	static void throwInvalid(const char* shortArgName, const char* message = "Invalid parameter")
	{
		const auto longArgName = SyntaxFactory::get().longArgName(shortArgName);
		const auto usageStr = SyntaxFactory::get().usage();

		throw MayaException(MStatus::kInvalidParameter,
			formatted("%s -%s (%s)\nUsage:\n%s", message, shortArgName, longArgName, usageStr));
	}

	private:
	MArgDatabase adb;
};

Arguments::Arguments(const MArgList& args, const MSyntax& syntax)
{
	// ReSharper disable CppExpressionWithoutSideEffects

	MStatus status;
	ArgChecker adb(syntax, args, status);

	adb.getObjects(selection);

	adb.required(flag::outputFolder, outputFolder);
	adb.optional(flag::scaleFactor, scaleFactor);

	glb = adb.isFlagSet(flag::glb);

	const path outputFolderPath(outputFolder.asChar());
	m_mayaOutputStream = adb.getOutputStream(flag::dumpMaya, "Maya debug", outputFolderPath, m_mayaOutputFileStream);
	m_gltfOutputStream = adb.getOutputStream(flag::dumpGLTF, "glTF debug", outputFolderPath, m_gltfOutputFileStream);

	dumpMaya = m_mayaOutputStream.get();
	dumpGLTF = m_gltfOutputStream.get();

	separate = adb.isFlagSet(flag::separate);
	defaultMaterial = adb.isFlagSet(flag::defaultMaterial);
	colorizeMaterials = adb.isFlagSet(flag::colorizeMaterials);
	forcePbrMaterials = adb.isFlagSet(flag::forcePbrMaterials);
	force32bitIndices = adb.isFlagSet(flag::force32bitIndices);
	assignObjectNames = adb.isFlagSet(flag::assignObjectNames);
	skipSkinClusters = adb.isFlagSet(flag::skipSkinClusters);
	skipBlendShapes = adb.isFlagSet(flag::skipBlendShapes);

	adb.optional(flag::globalOpacityFactor, opacityFactor);

	if (!adb.optional(flag::sceneName, sceneName))
	{
		// Use filename without extension of current scene file.
		MFileIO fileIO;
		const auto currentFilePath = MFileIO::currentFile();

		MFileObject fileObj;
		fileObj.setFullName(currentFilePath);

		// Remove extension from filename. I really miss C#!
		std::string fileName(fileObj.name().asChar());

		const auto lastindex = fileName.find_last_of('.');
		sceneName = fileName.substr(0, lastindex).c_str();
	}

	mikkelsenTangentAngularThreshold = adb.isFlagSet(flag::mikkelsenTangentSpace) ? 180.0f : 0.0f;
	adb.optional(flag::mikkelsenTangentAngularThreshold, mikkelsenTangentAngularThreshold);

	debugTangentVectors = adb.isFlagSet(flag::debugTangentVectors);
	debugNormalVectors = adb.isFlagSet(flag::debugNormalVectors);

	adb.optional(flag::debugVectorLength, debugVectorLength);

	// For debugging, dump some arguments again
	MStringArray selectedObjects;
	status = selection.getSelectionStrings(selectedObjects);
	THROW_ON_FAILURE(status);

	// By default use the current time for the initial values.
	initialValuesTime = MAnimControl::currentTime();
	adb.optional(flag::initialValuesTime, initialValuesTime);

	adb.optional(flag::redrawViewport, redrawViewport);

	// Parse mesh deformers to ignore
	const auto deformerNameCount = adb.flagUsageCount(flag::ignoreMeshDeformers);
	for (auto deformerNameIndex = 0; deformerNameIndex < deformerNameCount; ++deformerNameIndex)
	{
		MString deformerName;
		adb.required(flag::ignoreMeshDeformers, deformerName, deformerNameIndex);
		ignoreMeshDeformers.add(deformerName);
	}

	// Parse mesh primitive attributes
	MString attrs;
	if (adb.optional(flag::meshPrimitiveAttributes, attrs))
	{
		MStringArray parts;
		status = attrs.split('|', parts);
		ArgChecker::throwOnArgument(status, flag::meshPrimitiveAttributes);
		for (auto i = 0U; i < parts.length(); ++i)
		{
			const auto kind = Semantic::parse(parts[i].asChar());
			meshPrimitiveAttributes[kind] = true;
		}
	}
	else
	{
		for (auto kind: Semantic::kinds())
		{
			meshPrimitiveAttributes[kind] = true;
		}
	}

	// Parse animation clips
	const auto clipCount = adb.flagUsageCount(flag::animationClipNames);
	animationClips.reserve(clipCount);

	const auto fpsCount = adb.flagUsageCount(flag::animationClipFrameRate);

	for (int clipIndex = 0; clipIndex < clipCount; ++clipIndex)
	{
		double fps;
		adb.required(flag::animationClipFrameRate, fps, fpsCount == 1 ? 0 : clipIndex);

		MString name;
		adb.required(flag::animationClipNames, name, clipIndex);

		MTime start;
		adb.required(flag::animationClipStartTime, start, clipIndex);

		MTime end;
		adb.required(flag::animationClipEndTime, end, clipIndex);

		animationClips.emplace_back(name.asChar(), start, end, fps);
	}

	// Sort clips by starting time.
	std::sort(animationClips.begin(), animationClips.end(),
		[](const AnimClipArg& left, const AnimClipArg& right)
		{
			return left.startTime < right.endTime;
		});

	cout << prefix << "Exporting " << selectedObjects << " to " << outputFolder << "/" << sceneName << "..." << endl;
}

Arguments::~Arguments()
{
	if (m_mayaOutputFileStream.is_open())
	{
		m_mayaOutputFileStream.flush();
		m_mayaOutputFileStream.close();
	}

	if (m_gltfOutputFileStream.is_open())
	{
		m_gltfOutputFileStream.flush();
		m_gltfOutputFileStream.close();
	}
}

