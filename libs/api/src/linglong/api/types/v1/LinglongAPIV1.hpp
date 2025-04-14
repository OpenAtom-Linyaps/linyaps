// This file is generated by tools/codegen.sh
// DO NOT EDIT IT.

// clang-format off

//  To parse this JSON data, first install
//
//      json.hpp  https://github.com/nlohmann/json
//
//  Then include this file, and then do
//
//     LinglongAPIV1.hpp data = nlohmann::json::parse(jsonString);

#pragma once

#include <optional>
#include <nlohmann/json.hpp>
#include "linglong/api/types/v1/helper.hpp"

#include "linglong/api/types/v1/ApplicationConfiguration.hpp"
#include "linglong/api/types/v1/ApplicationConfigurationPermissions.hpp"
#include "linglong/api/types/v1/ApplicationPermissionsRequest.hpp"
#include "linglong/api/types/v1/BuilderConfig.hpp"
#include "linglong/api/types/v1/BuilderProject.hpp"
#include "linglong/api/types/v1/CliContainer.hpp"
#include "linglong/api/types/v1/CommonOptions.hpp"
#include "linglong/api/types/v1/CommonResult.hpp"
#include "linglong/api/types/v1/ContainerProcessStateInfo.hpp"
#include "linglong/api/types/v1/DialogHandShakePayload.hpp"
#include "linglong/api/types/v1/DialogMessage.hpp"
#include "linglong/api/types/v1/ExportDirs.hpp"
#include "linglong/api/types/v1/InspectResult.hpp"
#include "linglong/api/types/v1/InteractionReply.hpp"
#include "linglong/api/types/v1/InteractionRequest.hpp"
#include "linglong/api/types/v1/LayerInfo.hpp"
#include "linglong/api/types/v1/OciConfigurationPatch.hpp"
#include "linglong/api/types/v1/PackageInfo.hpp"
#include "linglong/api/types/v1/PackageInfoV2.hpp"
#include "linglong/api/types/v1/PackageManager1ExportParameters.hpp"
#include "linglong/api/types/v1/PackageManager1GetRepoInfoResult.hpp"
#include "linglong/api/types/v1/PackageManager1InstallParameters.hpp"
#include "linglong/api/types/v1/PackageManager1JobInfo.hpp"
#include "linglong/api/types/v1/PackageManager1ModifyRepoParameters.hpp"
#include "linglong/api/types/v1/PackageManager1Package.hpp"
#include "linglong/api/types/v1/PackageManager1PackageExportTaskResult.hpp"
#include "linglong/api/types/v1/PackageManager1PackageTaskResult.hpp"
#include "linglong/api/types/v1/PackageManager1RequestInteractionAdditionalMessage.hpp"
#include "linglong/api/types/v1/PackageManager1SearchParameters.hpp"
#include "linglong/api/types/v1/PackageManager1SearchResult.hpp"
#include "linglong/api/types/v1/PackageManager1UninstallParameters.hpp"
#include "linglong/api/types/v1/PackageManager1UpdateParameters.hpp"
#include "linglong/api/types/v1/Repo.hpp"
#include "linglong/api/types/v1/RepoConfig.hpp"
#include "linglong/api/types/v1/RepoConfigV2.hpp"
#include "linglong/api/types/v1/RepositoryCache.hpp"
#include "linglong/api/types/v1/UabMetaInfo.hpp"
#include "linglong/api/types/v1/UpgradeListResult.hpp"
#include "linglong/api/types/v1/XdgDirectoryPermission.hpp"

namespace linglong {
namespace api {
namespace types {
namespace v1 {
enum class InteractionMessageType : int;
enum class State : int;
enum class SubState : int;
}
}
}
}

namespace linglong {
namespace api {
namespace types {
namespace v1 {
/**
* Types used as v1 API of linglong D-Bus service, configuration files and CLI output. The
* top level type is a place holder to make quicktype work.
*/

using nlohmann::json;

/**
* Types used as v1 API of linglong D-Bus service, configuration files and CLI output. The
* top level type is a place holder to make quicktype work.
*/
struct LinglongAPIV1 {
std::optional<ApplicationConfiguration> applicationConfiguration;
std::optional<ApplicationConfigurationPermissions> applicationConfigurationPermissions;
std::optional<ApplicationPermissionsRequest> applicationPermissionsRequest;
std::optional<BuilderConfig> builderConfig;
std::optional<BuilderProject> builderProject;
std::optional<CliContainer> cliContainer;
std::optional<CommonOptions> commonOptions;
std::optional<CommonResult> commonResult;
std::optional<ContainerProcessStateInfo> containerProcessStateInfo;
std::optional<DialogHandShakePayload> dialogHandShakePayload;
std::optional<DialogMessage> dialogMessage;
std::optional<ExportDirs> exportDirs;
std::optional<InspectResult> inspectResult;
std::optional<InteractionMessageType> interactionMessageType;
std::optional<InteractionReply> interactionReply;
std::optional<InteractionRequest> interactionRequest;
std::optional<LayerInfo> layerInfo;
std::optional<OciConfigurationPatch> ociConfigurationPatch;
std::optional<PackageInfo> packageInfo;
std::optional<PackageInfoV2> packageInfoV2;
std::optional<PackageManager1ExportParameters> packageManager1ExportParameters;
std::optional<PackageManager1GetRepoInfoResult> packageManager1GetRepoInfoResult;
std::optional<CommonResult> packageManager1InstallLayerFDResult;
std::optional<PackageManager1InstallParameters> packageManager1InstallParameters;
std::optional<PackageManager1JobInfo> packageManager1JobInfo;
std::optional<PackageManager1ModifyRepoParameters> packageManager1ModifyRepoParameters;
std::optional<CommonResult> packageManager1ModifyRepoResult;
std::optional<PackageManager1Package> packageManager1Package;
std::optional<PackageManager1PackageExportTaskResult> packageManager1PackageExportTaskResult;
std::optional<PackageManager1PackageTaskResult> packageManager1PackageTaskResult;
std::optional<PackageManager1RequestInteractionAdditionalMessage> packageManager1RequestInteractionAdditionalMessage;
std::optional<PackageManager1SearchParameters> packageManager1SearchParameters;
std::optional<PackageManager1SearchResult> packageManager1SearchResult;
std::optional<PackageManager1UninstallParameters> packageManager1UninstallParameters;
std::optional<PackageManager1UpdateParameters> packageManager1UpdateParameters;
std::optional<Repo> repo;
std::optional<RepoConfig> repoConfig;
std::optional<RepoConfigV2> repoConfigV2;
std::optional<RepositoryCache> repositoryCache;
std::optional<State> state;
std::optional<SubState> subState;
std::optional<UabMetaInfo> uabMetaInfo;
std::optional<UpgradeListResult> upgradeListResult;
std::optional<std::vector<XdgDirectoryPermission>> xdgDirectoryPermissions;
};
}
}
}
}

// clang-format on
