#include "GLDisplay.h"

void GLDisplay::SetNESDevice(NESDevice* nesDevice)
{
	this->m_NESDevicePtr = nesDevice;
}

void GLDisplay::Initialize()
{
	//----------------------------------------------------------------
	//Init screen texture
	glGenTextures(1, &m_GLDisplayTexture);
	glBindTexture(GL_TEXTURE_2D, m_GLDisplayTexture);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glTexImage2D(
		GL_TEXTURE_2D,
		0,
		GL_RGB,
		DISPLAY_TEXTURE_WIDTH,
		DISPLAY_TEXTURE_HEIGHT,
		0,
		GL_RGB,
		GL_UNSIGNED_BYTE,
		nullptr);

	//Init PBO for transfer pixel data to texture
	glGenBuffers(1, &m_GLDisplayPBO);
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, m_GLDisplayPBO);
	glBufferData(
		GL_PIXEL_UNPACK_BUFFER,
		DISPLAY_TEXTURE_BUFFER_SIZE,
		nullptr,
		GL_STREAM_DRAW);

	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
	//----------------------------------------------------------------
	//Init table_id textures (yep, code exactly the same as for display init)
	for (int table_id = 0; table_id < 2; table_id++)
	{
		glGenTextures(1, &m_GLPatternTexture[table_id]);
		glBindTexture(GL_TEXTURE_2D, m_GLPatternTexture[table_id]);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

		glTexImage2D(
			GL_TEXTURE_2D,
			0,
			GL_RGB,
			PATTERN_TEXTURE_WIDTH,
			PATTERN_TEXTURE_HEIGHT,
			0,
			GL_RGB,
			GL_UNSIGNED_BYTE,
			nullptr);

		//Init PBO for transfer pixel data to texture
		glGenBuffers(1, &m_GLPatternPBO[table_id]);
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, m_GLPatternPBO[table_id]);
		glBufferData(
			GL_PIXEL_UNPACK_BUFFER,
			PATTERN_TEXTURE_BUFFER_SIZE,
			nullptr,
			GL_STREAM_DRAW);

		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
	}
	//----------------------------------------------------------------
	//Init nametables texture (yep, another simmilar chunk of code)
	glGenTextures(1, &m_GLNametablesTexture);
	glBindTexture(GL_TEXTURE_2D, m_GLNametablesTexture);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glTexImage2D(
		GL_TEXTURE_2D,
		0,
		GL_RGB,
		NAMETABLES_TEXTURE_WIDTH,
		NAMETABLES_TEXTURE_HEIGHT,
		0,
		GL_RGB,
		GL_UNSIGNED_BYTE,
		nullptr);

	//Init PBO for transfer pixel data to texture
	glGenBuffers(1, &m_GLNametablesPBO);
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, m_GLNametablesPBO);
	glBufferData(
		GL_PIXEL_UNPACK_BUFFER,
		NAMETABLES_TEXTURE_BUFFER_SIZE,
		nullptr,
		GL_STREAM_DRAW);

	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
}

void GLDisplay::Destroy()
{
	glDeleteTextures(1, &m_GLDisplayTexture);
	glDeleteBuffers(1, &m_GLDisplayPBO);

	glDeleteTextures(2, m_GLPatternTexture);
	glDeleteBuffers(2, m_GLPatternPBO);

	glDeleteTextures(1, &m_GLNametablesTexture);
	glDeleteBuffers(1, &m_GLNametablesPBO);
}

GLuint GLDisplay::getDisplayTexture()
{
	return m_GLDisplayTexture;
}

GLfloat GLDisplay::getDisplayWidth()
{
	return DISPLAY_TEXTURE_WIDTH;
}

GLfloat GLDisplay::getDisplayHeight()
{
	return DISPLAY_TEXTURE_HEIGHT;
}

GLuint GLDisplay::getPatternTexture(uint32_t id)
{
	return m_GLPatternTexture[id];
}
GLuint GLDisplay::getNametablesTexture()
{
	return m_GLNametablesTexture;
}
void GLDisplay::Update()
{
	UpdateDisplayTexture();
}

void GLDisplay::UpdateDisplayTexture()
{
	//---------------------------------------------------------------------------------------
	glBindTexture(GL_TEXTURE_2D, m_GLDisplayTexture);
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, m_GLDisplayPBO);
	//---------------------------------------------------------------------------------------
	
	//Update PBO buffer
	glBufferData(
		GL_PIXEL_UNPACK_BUFFER,
		DISPLAY_TEXTURE_BUFFER_SIZE,
		m_NESDevicePtr->GetPPU().GetFramebuffer(),
		GL_STREAM_DRAW
	);
	
	//Update texture from pbo
	glBindTexture(GL_TEXTURE_2D, m_GLDisplayTexture);
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, m_GLDisplayPBO);
	glTexSubImage2D(
		GL_TEXTURE_2D,
		0,
		0,
		0,
		DISPLAY_TEXTURE_WIDTH,
		DISPLAY_TEXTURE_HEIGHT,
		GL_RGB,
		GL_UNSIGNED_BYTE,
		nullptr);

	//---------------------------------------------------------------------------------------
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
	//---------------------------------------------------------------------------------------
}

void GLDisplay::UpdatePatternTexture(uint8_t id, uint8_t palette)
{
	id &= 0x1;
	//---------------------------------------------------------------------------------------
	glBindTexture(GL_TEXTURE_2D, m_GLPatternTexture[id]);
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, m_GLPatternPBO[id]);
	//---------------------------------------------------------------------------------------
	//Update PBO buffer
	glBufferData(
		GL_PIXEL_UNPACK_BUFFER,
		PATTERN_TEXTURE_BUFFER_SIZE,
		m_NESDevicePtr->GetPPU().ResterizePatterntable(id, palette),
		GL_STREAM_DRAW
	);
	//Update texture from pbo
	glBindTexture(GL_TEXTURE_2D, m_GLPatternTexture[id]);
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, m_GLPatternPBO[id]);
	glTexSubImage2D(
		GL_TEXTURE_2D,
		0,
		0,
		0,
		PATTERN_TEXTURE_WIDTH,
		PATTERN_TEXTURE_HEIGHT,
		GL_RGB,
		GL_UNSIGNED_BYTE,
		nullptr);
	//---------------------------------------------------------------------------------------
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
	//---------------------------------------------------------------------------------------
}

void GLDisplay::UpdateNametables()
{
	//---------------------------------------------------------------------------------------
	glBindTexture(GL_TEXTURE_2D, m_GLNametablesTexture);
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, m_GLNametablesPBO);
	//---------------------------------------------------------------------------------------

	//Update PBO buffer
	glBufferData(
		GL_PIXEL_UNPACK_BUFFER,
		NAMETABLES_TEXTURE_BUFFER_SIZE,
		m_NESDevicePtr->GetPPU().ResterizeNametables(),
		GL_STREAM_DRAW
	);

	//Update texture from pbo
	glBindTexture(GL_TEXTURE_2D, m_GLNametablesTexture);
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, m_GLNametablesPBO);
	glTexSubImage2D(
		GL_TEXTURE_2D,
		0,
		0,
		0,
		NAMETABLES_TEXTURE_WIDTH,
		NAMETABLES_TEXTURE_HEIGHT,
		GL_RGB,
		GL_UNSIGNED_BYTE,
		nullptr);

	//---------------------------------------------------------------------------------------
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
	//---------------------------------------------------------------------------------------
}