#pragma once

#include <iostream>
#include <string>
#include <thread>
#include <condition_variable>
#include <cmath>
#include <iterator>
#include <Eigen/Core>
#include <Eigen/Geometry> 
#include <SDL.h>
#include <SDL_opengl.h> 
#include <chrono>
#include <render.h>
#include <render_threading.h>

namespace render_helper
{
	int reterr(int aRet, std::string aMsg)
	{
		std::cerr << aMsg << std::endl;
		return aRet;
	}

	template<typename tLoopFunc>
	int start_sdl2_loop(size_t aWidth, size_t aHeight, tLoopFunc loop_func)
	{
		// Initialize SDL with video
		SDL_Init(SDL_INIT_VIDEO);

		// Create an SDL window
		SDL_Window* window = SDL_CreateWindow("Test", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, aWidth, aHeight, SDL_WINDOW_OPENGL);
		if (!window) {
			return reterr(1, "Error failed to create window!");
		}

		// Create an OpenGL context (so we can use OpenGL functions)
		SDL_GLContext context = SDL_GL_CreateContext(window);
		if (!context) {
			return reterr(2, "Error failed to create a context!");
		}

		SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
		if (!renderer) {
			return reterr(3, "Could not create renderer");
		}

		SDL_Texture* buffer = SDL_CreateTexture(renderer,
			SDL_PIXELFORMAT_ARGB8888,
			SDL_TEXTUREACCESS_STATIC,
			aWidth,
			aHeight);

		auto fps = std::chrono::steady_clock::now();
		int frame_count = 0;

		while (true) {
			if (0 != loop_func(window, context, renderer, buffer))
				break;

			//fps
			{
				frame_count++;

				const auto fps_end = std::chrono::steady_clock::now();
				const auto fps_diff = fps_end - fps;
				const auto fps_diff_sec = std::chrono::duration<double, std::milli>(fps_diff).count();

				if (fps_diff_sec > 1000.0) {
					std::string caption = std::to_string(frame_count) + " Frames";
					SDL_SetWindowTitle(window, caption.c_str());
					frame_count = 0;

					fps = std::chrono::steady_clock::now();
				}
			}
		}

		// Destroy the context
		SDL_GL_DeleteContext(context);

		// Destroy the window
		SDL_DestroyWindow(window);

		// And quit SDL
		SDL_Quit();

		return 0;
	}
}