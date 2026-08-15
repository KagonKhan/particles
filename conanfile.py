import os

from conan import ConanFile
from conan.tools.cmake import cmake_layout
from conan.tools.files import copy
from conan.tools.system.package_manager import Apt


class ImGuiExample(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeDeps", "CMakeToolchain"

    # onetbb's recipe refuses to build against a static hwloc, which it pulls in for the
    # core-binding it does on machines with more than one cache domain.
    default_options = {"hwloc/*:shared": True}


    def requirements(self):
        self.requires("imgui/1.92.9b-docking")
        self.requires("glfw/3.4")
        self.requires("glew/2.2.0")
        self.requires("stb/cci.20240531")
        self.requires("spdlog/1.17.0")
        self.requires("glm/1.0.3")
        self.requires("nlohmann_json/3.12.0")

        # libstdc++ implements the parallel execution policies on top of TBB. Without it
        # <execution> does not compile, and where it does compile without it, par silently
        # runs serially — so this is a hard dependency of the parallel toggle, not an
        # optimization of it.
        self.requires("onetbb/2021.12.0")
        
        
    def system_requirements(self):
        packages = [
            "pkg-config"
        ]

        Apt(self).install(packages)


    def generate(self):
        copy(self, "*glfw*", os.path.join(self.dependencies["imgui"].package_folder,
            "res", "bindings"), os.path.join(self.source_folder, "bindings/imgui"))
        copy(self, "*opengl3*", os.path.join(self.dependencies["imgui"].package_folder,
            "res", "bindings"), os.path.join(self.source_folder, "bindings/imgui"))


    def layout(self):
        cmake_layout(self)