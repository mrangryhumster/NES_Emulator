#pragma once

#include "GL/gl3w.h"
#include "NESDevice.h"

#define DISPLAY_TEXTURE_WIDTH 256
#define DISPLAY_TEXTURE_HEIGHT 256
#define DISPLAY_TEXTURE_PIXEL_SIZE 3
#define DISPLAY_TEXTURE_BUFFER_SIZE DISPLAY_TEXTURE_WIDTH * DISPLAY_TEXTURE_HEIGHT * DISPLAY_TEXTURE_PIXEL_SIZE

#define PATTERN_TEXTURE_WIDTH 128
#define PATTERN_TEXTURE_HEIGHT 128
#define PATTERN_TEXTURE_PIXEL_SIZE 3
#define PATTERN_TEXTURE_BUFFER_SIZE PATTERN_TEXTURE_WIDTH * PATTERN_TEXTURE_HEIGHT * PATTERN_TEXTURE_PIXEL_SIZE

#define NAMETABLES_TEXTURE_WIDTH 512
#define NAMETABLES_TEXTURE_HEIGHT 512
#define NAMETABLES_TEXTURE_PIXEL_SIZE 3
#define NAMETABLES_TEXTURE_BUFFER_SIZE NAMETABLES_TEXTURE_WIDTH * NAMETABLES_TEXTURE_HEIGHT * NAMETABLES_TEXTURE_PIXEL_SIZE

class GLDisplay
{
public:
	void SetNESDevice(NESDevice* nesDevice);

	void Initialize();
	void Destroy();

	void Update();

	void UpdateDisplayTexture();
	void UpdatePatternTexture(uint8_t id,uint8_t palette);
	void UpdateNametables();

	GLuint  getDisplayTexture();
	GLfloat getDisplayWidth();
	GLfloat getDisplayHeight();

	GLuint getPatternTexture(uint32_t id);
	GLuint getNametablesTexture();

protected:

	NESDevice* m_NESDevicePtr;
	//--------------------------------
	GLuint		m_GLDisplayTexture;
	GLuint		m_GLDisplayPBO;
	//--------------------------------
	GLuint		m_GLPatternTexture[2];
	GLuint		m_GLPatternPBO[2];
	//--------------------------------
	GLuint		m_GLNametablesTexture;
	GLuint		m_GLNametablesPBO;
	//--------------------------------

};