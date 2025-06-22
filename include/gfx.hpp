#pragma once
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <utility>
#include <iostream>
#include <cstdlib>


struct Window {
	int width;
	int height;
	GLFWwindow *handle;

	Window(const char *title, int width, int height)
		: width(width), height(height)
	{
		int status;
		status = glfwInit();
		if (status == -1) {
			std::cerr << "[OpenGL] Initialization failed.\n";
			std::exit(1);
		}
		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
		handle = glfwCreateWindow(width, height, title, nullptr, nullptr);
		glfwMakeContextCurrent(handle);
		status = gladLoadGLLoader((GLADloadproc) glfwGetProcAddress);
		if (status == -1) {
			std::cerr << "[GLAD] OpenGL load failed.\n";
			std::exit(1);
		}
		glViewport(0, 0, width, height);
	}

	Window(Window &&other)
		: width(other.width),
		height(other.height),
		handle(std::exchange(other.handle, nullptr))
	{
	}

	~Window()
	{
		if (handle) {
			glfwDestroyWindow(handle);
			glfwTerminate();
		}
	}
};

