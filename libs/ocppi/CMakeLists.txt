option(OCPPI_WITH_SPDLOG "Build ocppi with spdlog or not." OFF)

if(OCPPI_WITH_SPDLOG)
  set(OCPPI_SPDLOG_VERSION 1.3.1) # debian buster
  list(APPEND OCPPI_COMPILE_OPTIONS PUBLIC -DOCPPI_WITH_SPDLOG)
  list(APPEND OCPPI_LINK_LIBRARIES PUBLIC spdlog::spdlog)
  list(APPEND OCPPI_DEPENDENCIES PUBLIC "spdlog 1.3.1 REQUIRED")
endif()

if(CMAKE_VERSION LESS 3.14)
  message("CPM.cmake disabled")
else()
  if(OCPPI_WITH_SPDLOG)
    CPMFindPackage(
      NAME spdlog
      VERSION ${OCPPI_SPDLOG_VERSION}
      GITHUB_REPOSITORY "gabime/spdlog"
      GIT_TAG "v1.3.1"
      GIT_SHALLOW ON
      EXCLUDE_FROM_ALL ON
      OPTIONS
        "CMAKE_POSITION_INDEPENDENT_CODE ON" # NOTE:
                                             # https://github.com/gabime/spdlog/issues/1190
    )
  endif()
endif()

list(APPEND OCPPI_LINK_LIBRARIES PUBLIC tl::expected)
list(APPEND OCPPI_DEPENDENCIES PUBLIC "tl-expected 1.0.0 REQUIRED")

list(APPEND OCPPI_LINK_LIBRARIES PUBLIC nlohmann_json::nlohmann_json)
list(APPEND OCPPI_DEPENDENCIES PUBLIC "nlohmann_json 3.5.0 REQUIRED")

set(OCPPI_OCI_RUNTIME_SPEC_MIN "1.0.0")
set(OCPPI_OCI_RUNTIME_SPEC_MAX "1.1.0")

pfl_add_library(
  DISABLE_INSTALL
  LIBRARY_TYPE
  STATIC
  OUTPUT_NAME
  ocppi
  EXAMPLES
  parse-config
  using-crun
  with-logger
  LINK_LIBRARIES
  ${OCPPI_LINK_LIBRARIES}
  PUBLIC
  stdc++fs
  COMPILE_FEATURES
  PUBLIC
  cxx_std_17
  COMPILE_OPTIONS
  ${OCPPI_COMPILE_OPTIONS}
  DEPENDENCIES
  ${OCPPI_DEPENDENCIES}
  SOURCES
  # find -regex '\./src/.+\.[ch]\(pp\)?(\.in)?' -type f -printf '%P\n'| sort
  include/ocppi/cli/CLI.hpp
  include/ocppi/cli/CommandFailedError.hpp
  include/ocppi/cli/CommonCLI.hpp
  include/ocppi/cli/crun/Crun.hpp
  include/ocppi/cli/ExecutableNotFoundError.hpp
  include/ocppi/cli/runc/Runc.hpp
  include/ocppi/cli/youki/Youki.hpp
  include/ocppi/configure.hpp.in
  include/ocppi/InvalidArgumentError.hpp
  include/ocppi/runtime/config/types/Anet.hpp
  include/ocppi/runtime/config/types/BlockIODeviceThrottle.hpp
  include/ocppi/runtime/config/types/BlockIODeviceWeight.hpp
  include/ocppi/runtime/config/types/BlockIO.hpp
  include/ocppi/runtime/config/types/BoottimeClass.hpp
  include/ocppi/runtime/config/types/Capabilities.hpp
  include/ocppi/runtime/config/types/CappedCPU.hpp
  include/ocppi/runtime/config/types/CappedMemory.hpp
  include/ocppi/runtime/config/types/Class.hpp
  include/ocppi/runtime/config/types/Config.hpp
  include/ocppi/runtime/config/types/ConsoleSize.hpp
  include/ocppi/runtime/config/types/DeviceCgroup.hpp
  include/ocppi/runtime/config/types/FluffyCPU.hpp
  include/ocppi/runtime/config/types/FluffyMemory.hpp
  include/ocppi/runtime/config/types/Generators.hpp
  include/ocppi/runtime/config/types/helper.hpp
  include/ocppi/runtime/config/types/Hook.hpp
  include/ocppi/runtime/config/types/Hooks.hpp
  include/ocppi/runtime/config/types/HugepageLimit.hpp
  include/ocppi/runtime/config/types/Hyperv.hpp
  include/ocppi/runtime/config/types/Hypervisor.hpp
  include/ocppi/runtime/config/types/IdMapping.hpp
  include/ocppi/runtime/config/types/IdType.hpp
  include/ocppi/runtime/config/types/Image.hpp
  include/ocppi/runtime/config/types/IntelRdt.hpp
  include/ocppi/runtime/config/types/IoPriority.hpp
  include/ocppi/runtime/config/types/Kernel.hpp
  include/ocppi/runtime/config/types/LinuxDevice.hpp
  include/ocppi/runtime/config/types/Linux.hpp
  include/ocppi/runtime/config/types/LinuxResources.hpp
  include/ocppi/runtime/config/types/Mount.hpp
  include/ocppi/runtime/config/types/NamespaceReference.hpp
  include/ocppi/runtime/config/types/NamespaceType.hpp
  include/ocppi/runtime/config/types/NetworkInterfacePriority.hpp
  include/ocppi/runtime/config/types/PersonalityDomain.hpp
  include/ocppi/runtime/config/types/Personality.hpp
  include/ocppi/runtime/config/types/Pids.hpp
  include/ocppi/runtime/config/types/Process.hpp
  include/ocppi/runtime/config/types/PurpleCPU.hpp
  include/ocppi/runtime/config/types/PurpleMemory.hpp
  include/ocppi/runtime/config/types/Rdma.hpp
  include/ocppi/runtime/config/types/ResourcesNetwork.hpp
  include/ocppi/runtime/config/types/Rlimit.hpp
  include/ocppi/runtime/config/types/RootfsPropagation.hpp
  include/ocppi/runtime/config/types/Root.hpp
  include/ocppi/runtime/config/types/RootImageFormat.hpp
  include/ocppi/runtime/config/types/SchedulerFlag.hpp
  include/ocppi/runtime/config/types/Scheduler.hpp
  include/ocppi/runtime/config/types/SchedulerPolicy.hpp
  include/ocppi/runtime/config/types/SeccompAction.hpp
  include/ocppi/runtime/config/types/SeccompArch.hpp
  include/ocppi/runtime/config/types/SeccompFlag.hpp
  include/ocppi/runtime/config/types/Seccomp.hpp
  include/ocppi/runtime/config/types/SeccompOperators.hpp
  include/ocppi/runtime/config/types/Solaris.hpp
  include/ocppi/runtime/config/types/Storage.hpp
  include/ocppi/runtime/config/types/SyscallArg.hpp
  include/ocppi/runtime/config/types/Syscall.hpp
  include/ocppi/runtime/config/types/TimeOffsets.hpp
  include/ocppi/runtime/config/types/User.hpp
  include/ocppi/runtime/config/types/Vm.hpp
  include/ocppi/runtime/config/types/WindowsDevice.hpp
  include/ocppi/runtime/config/types/Windows.hpp
  include/ocppi/runtime/config/types/WindowsNetwork.hpp
  include/ocppi/runtime/config/types/WindowsResources.hpp
  include/ocppi/runtime/config/types/ZosDevice.hpp
  include/ocppi/runtime/config/types/Zos.hpp
  include/ocppi/runtime/configure.hpp.in
  include/ocppi/runtime/ContainerID.hpp
  include/ocppi/runtime/CreateOption.hpp
  include/ocppi/runtime/DeleteOption.hpp
  include/ocppi/runtime/ExecOption.hpp
  include/ocppi/runtime/FeaturesOption.hpp
  include/ocppi/runtime/features/types/Apparmor.hpp
  include/ocppi/runtime/features/types/Cgroup.hpp
  include/ocppi/runtime/features/types/Features.hpp
  include/ocppi/runtime/features/types/Generators.hpp
  include/ocppi/runtime/features/types/helper.hpp
  include/ocppi/runtime/features/types/IntelRdt.hpp
  include/ocppi/runtime/features/types/Linux.hpp
  include/ocppi/runtime/features/types/NamespaceType.hpp
  include/ocppi/runtime/features/types/SeccompAction.hpp
  include/ocppi/runtime/features/types/SeccompArch.hpp
  include/ocppi/runtime/features/types/SeccompFlag.hpp
  include/ocppi/runtime/features/types/Seccomp.hpp
  include/ocppi/runtime/features/types/SeccompOperators.hpp
  include/ocppi/runtime/features/types/Selinux.hpp
  include/ocppi/runtime/GlobalOption.hpp
  include/ocppi/runtime/KillOption.hpp
  include/ocppi/runtime/ListOption.hpp
  include/ocppi/runtime/RunOption.hpp
  include/ocppi/runtime/Runtime.hpp
  include/ocppi/runtime/Signal.hpp
  include/ocppi/runtime/SpecRuntime.hpp
  include/ocppi/runtime/StartOption.hpp
  include/ocppi/runtime/StateOption.hpp
  include/ocppi/runtime/state/types/Generators.hpp
  include/ocppi/runtime/state/types/helper.hpp
  include/ocppi/runtime/state/types/State.hpp
  include/ocppi/runtime/state/types/Status.hpp
  include/ocppi/types/ContainerListItem.hpp
  include/ocppi/types/Generators.hpp
  include/ocppi/types/helper.hpp
  src/ocppi/cli/CLI.cpp
  src/ocppi/cli/CommandFailedError.cpp
  src/ocppi/cli/CommonCLI.cpp
  src/ocppi/cli/crun/Crun.cpp
  src/ocppi/cli/ExecutableNotFoundError.cpp
  src/ocppi/cli/Process.cpp
  src/ocppi/cli/Process.hpp
  src/ocppi/cli/runc/Runc.cpp
  src/ocppi/cli/youki/Youki.cpp
  src/ocppi/configure.cpp
  src/ocppi/InvalidArgumentError.cpp
  src/ocppi/runtime/configure.cpp
  src/ocppi/runtime/ContainerID.cpp
  src/ocppi/runtime/CreateOption.cpp
  src/ocppi/runtime/DeleteOption.cpp
  src/ocppi/runtime/FeaturesOption.cpp
  src/ocppi/runtime/GlobalOption.cpp
  src/ocppi/runtime/KillOption.cpp
  src/ocppi/runtime/RunOption.cpp
  src/ocppi/runtime/Runtime.cpp
  src/ocppi/runtime/Signal.cpp
  src/ocppi/runtime/StartOption.cpp
  src/ocppi/runtime/StateOption.cpp)
