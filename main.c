/* standard headers */
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

/* SDL2 headers */
#include <SDL2/SDL.h>

/* constants */
#define WINDOW_WIDTH (900)
#define WINDOW_HEIGHT (600)
#define PI (3.141592653589793f)
#define G (0.01f)

/* typedefs */
typedef float float2 __attribute__((ext_vector_type(2)));
typedef struct state
{
	float2 r;
	float2 m;
	float2 a;
	float2 a_velocity;
	uint32_t *canvas;
} state_t;

/* from https://gist.github.com/derofim/912cfc9161269336f722#file-source-cpp-L98 */
static void fill_circle(SDL_Renderer *renderer, int32_t cx, int32_t cy, int32_t radius)
{
	for (int32_t dy = 1; dy <= radius; ++dy)
	{
		// This loop is unrolled a bit, only iterating through half of the
		// height of the circle.  The result is used to draw a scan line and
		// its mirror image below it.

		// The following formula has been simplified from our original.  We
		// are using half of the width of the circle because we are provided
		// with a center and we need left/right coordinates.

		float dx = floorf(sqrtf((2.0f * radius * dy) - (dy * dy)));
		SDL_RenderDrawLine(renderer, cx - dx, cy + dy - radius, cx + dx, cy + dy - radius);
		SDL_RenderDrawLine(renderer, cx - dx, cy - dy + radius, cx + dx, cy - dy + radius);
	}
}

static void update_state(state_t *this, SDL_Renderer *renderer, SDL_Texture *texture, uint32_t *canvas)
{
	/* draw the state */
	{
		/* clear background */
		SDL_SetRenderDrawColor(renderer, 255, 255, 255, 0);
		SDL_RenderClear(renderer);

		/* update texture */
		SDL_UpdateTexture(texture, 0, canvas, sizeof(uint32_t) * WINDOW_WIDTH);

		/* draw the canvas */
		SDL_RenderCopy(renderer, texture, 0, 0);

		/* set the drawing color to black */
		SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);

		/* draw the lines between the two points */
		float2 pos1 = this->r.x * (float2) { sinf(this->a.x), cosf(this->a.x) } + (float2) { WINDOW_WIDTH / 2, 50 };
		float2 pos2 = pos1 + this->r.y * (float2) { sinf(this->a.y), cosf(this->a.y) };
		SDL_RenderDrawLine(renderer, WINDOW_WIDTH / 2, 50, pos1.x, pos1.y);
	    SDL_RenderDrawLine(renderer, pos1.x, pos1.y, pos2.x, pos2.y);

		/* draw the two points */
		fill_circle(renderer, pos1.x, pos1.y, this->m.x);
		fill_circle(renderer, pos2.x, pos2.y, this->m.y);
		
		/* plot the second point on the canvas */
		if (floorf(pos2.x) >= 0 && floorf(pos2.x) < WINDOW_WIDTH
			&& floorf(pos2.y) >= 0 && floorf(pos2.y) < WINDOW_HEIGHT)
		{
			uint32_t *writing_pixel = &canvas[(uint32_t)(floorf(pos2.y) * WINDOW_WIDTH + floorf(pos2.x))];
			*writing_pixel = 255;
		}

		/* show renderer to screen */
		SDL_RenderPresent(renderer);
	}

	/* update the acceleration vector */
	float2 a_acceleration = { 0, 0 };
	{
		/* x */
		{
			float num1 = -G * (2 * this->m.x + this->m.y) * sinf(this->a.x);
			float num2 = -this->m.y * G * sinf(this->a.x - 2 * this->a.y);
			float num3 = -2 * sinf(this->a.x - this->a.y) * this->m.y;
			float num4 = this->a_velocity.y * this->a_velocity.y * this->r.y
				+ this->a_velocity.x * this->a_velocity.x * this->r.x * cosf(this->a.x - this->a.y);
			float den = this->r.x * (2 * this->m.x + this->m.y - this->m.y * cosf(2 * this->a.x - 2 * this->a.y));

			a_acceleration.x = (num1 + num2 + num3 * num4) / den;
		}

		/* y */
		{
			float num1 = 2 * sinf(this->a.x - this->a.y);
			float num2 = (this->a_velocity.x * this->a_velocity.y * this->r.x * (this->m.x + this->m.y));
			float num3 = G * (this->m.x + this->m.y) * cosf(this->a.x);
			float num4 = this->a_velocity.y * this->a_velocity.y * this->r.y * this->m.y * cosf(this->a.x-this->a.y);
			float den = this->r.x * (2 * this->m.x + this->m.y - this->m.y * cosf(2 * this->a.x - 2 * this->a.y));

			a_acceleration.y = (num1 * (num2 + num3 + num4)) / den;
		}
	}

	/* update a with the velocity and acceleration */
	{

		/* change the velocity by acceleration */
		this->a_velocity += a_acceleration;

		/* change a by velocity */
		this->a += this->a_velocity;
	}

}

int main(int argc, char *argv[])
{
	/* we don't need these */
	(void)argc;
	(void)argv;

	if (SDL_Init(SDL_INIT_VIDEO) < 0)
	{
		/* print error message to stderr */
		perror(SDL_GetError());
		return EXIT_FAILURE;
	}

	/* setup SDL */
#define create_SDL(type, var, ...) \
    type var = __VA_ARGS__;        \
    if (var == NULL)               \
    {                              \
        perror(SDL_GetError());	   \
        return EXIT_FAILURE;       \
    }

	create_SDL(SDL_Window *, window,
		SDL_CreateWindow("",
			SDL_WINDOWPOS_UNDEFINED,
			SDL_WINDOWPOS_UNDEFINED,
			WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_OPENGL));

	create_SDL(SDL_Renderer *, renderer,
		SDL_CreateRenderer(window, -1,
			SDL_RENDERER_ACCELERATED));

	create_SDL(SDL_Texture *, texture,
		SDL_CreateTexture(renderer,
			SDL_PIXELFORMAT_RGBA8888,
			SDL_TEXTUREACCESS_STREAMING,
			WINDOW_WIDTH, WINDOW_HEIGHT));

#undef create_SDL

	/* setup */
	bool quit = false;
	state_t state = {
		.r = {200, 200},
		.m = {20, 20},
		.a = { PI / 2, PI / 8  },
		/* create a canvas for keeping track of the path of the pendulum */
		.canvas = ({ uint32_t * canvas = malloc(sizeof(uint32_t) * WINDOW_WIDTH * WINDOW_HEIGHT);
					 /* fill the grid with white */
					 memset(canvas, 255, sizeof(uint32_t) * WINDOW_WIDTH * WINDOW_HEIGHT);
					 canvas;
		}) 
	};

	while (!quit)
	{
		for (SDL_Event event = { 0 }; SDL_PollEvent(&event); )
			switch (event.type)
			{
				case SDL_QUIT:
				{
					quit = true;
				} break;
			}

		/* draw to renderer */
		update_state(&state, renderer, texture, state.canvas);
	}

	/* cleanup SDL and free memory */
	free(state.canvas);
	SDL_DestroyTexture(texture);
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();
	return 0;
}
