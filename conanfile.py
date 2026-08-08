import os

from conan import ConanFile
from conan.tools.cmake import cmake_layout
from conan.tools.files import copy
from conan.tools.system.package_manager import Apt


class ImGuiExample(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeDeps", "CMakeToolchain"


    def requirements(self):
        self.requires("imgui/1.92.5-docking")
        self.requires("glfw/3.4")
        self.requires("glew/2.2.0")
        self.requires("stb/cci.20240531")
        self.requires("spdlog/1.17.0")
        
        
    def system_requirements(self):
        packages = [
            "pkg-config"
        ]

        Apt(self).install(packages)


    def generate(self):
        copy(self, "*glfw*", os.path.join(self.dependencies["imgui"].package_folder,
            "res", "bindings"), os.path.join(self.source_folder, "bindings"))
        copy(self, "*opengl3*", os.path.join(self.dependencies["imgui"].package_folder,
            "res", "bindings"), os.path.join(self.source_folder, "bindings"))


    def layout(self):
        cmake_layout(self)