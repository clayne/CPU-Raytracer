#pragma once
#include <GL/glew.h>
#include <SDL2/SDL.h>

#include "Vector3.h"

struct Window {
private:
	SDL_Window *  window;
	SDL_GLContext context;
	
	unsigned * frame_buffer;
	GLuint     frame_buffer_handle;

public:
	const int width;
	const int height;

	bool is_closed = false;

	Window(int width, int height, const char * title);
	~Window();

	// Clears the framebuffer
	void clear();

	void update();

	inline void plot(int x, int y, unsigned colour) const {
		frame_buffer[x + width * y] = colour;
	}

	inline void plot(int x, int y, const Vector3 & colour) const {
		int r = colour.x * 255.0f;
		int g = colour.y * 255.0f;
		int b = colour.z * 255.0f;
		frame_buffer[x + width * y] = (r << 16) | (g << 8) | b;
	}

	inline void set_title(const char * title) {
		SDL_SetWindowTitle(window, title);
	}
};
