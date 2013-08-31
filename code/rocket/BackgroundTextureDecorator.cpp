//
// Created by Sergei Shubin <s.v.shubin@gmail.com>
//

#include "BackgroundTextureDecorator.h"
#include <Rocket/Core.h>
#include <Rocket/Core/Element.h>
#include <Rocket/Core/SystemInterface.h>
#include "ShellOpenGL.h"
#include <math.h>

BackgroundTextureDecorator::~BackgroundTextureDecorator() {
}

bool BackgroundTextureDecorator::Initialise(const Rocket::Core::String& image_source, const Rocket::Core::String& image_path, GLint wrap_s, GLint wrap_t) {
    m_texture = LoadTexture(image_source, image_path);
    m_texture_id = 0;
    m_wrap_s = wrap_s;
    m_wrap_t = wrap_t;
    return m_texture != -1;
}

Rocket::Core::DecoratorDataHandle BackgroundTextureDecorator::GenerateElementData(Rocket::Core::Element *element) {
	return NULL;
}

void BackgroundTextureDecorator::ReleaseElementData(Rocket::Core::DecoratorDataHandle element_data) {
}

void BackgroundTextureDecorator::RenderElement(Rocket::Core::Element *element, Rocket::Core::DecoratorDataHandle element_data) {
    if (m_texture == -1) {
        return;
    }
    
	Rocket::Core::Vector2f position = element->GetAbsoluteOffset(Rocket::Core::Box::PADDING);
	Rocket::Core::Vector2f size = element->GetBox().GetSize(Rocket::Core::Box::PADDING);
	
	glEnable(GL_TEXTURE_2D);
	
    if (m_texture_id == 0) {
        m_texture_id = (GLuint) GetTexture(m_texture)->GetHandle(element->GetRenderInterface());
        glBindTexture(GL_TEXTURE_2D, m_texture_id);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &m_texture_width);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &m_texture_height);
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, m_wrap_s );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, m_wrap_t );
    } else {
        glBindTexture(GL_TEXTURE_2D, m_texture_id);
    }
    
    const Rocket::Core::Property *prop_offset_x = element->GetProperty("offset-x");
    const Rocket::Core::Property *prop_offset_y = element->GetProperty("offset-y");
    
    float offset_x = prop_offset_x == NULL ? 0.0f : prop_offset_x->Get<float>();
    float offset_y = prop_offset_y == NULL ? 0.0f : prop_offset_y->Get<float>();
    
    float l = offset_x/m_texture_width;
    float t = offset_y/m_texture_height;
    float w = size.x/m_texture_width;
    float h = size.y/m_texture_height;
    float r = l+w;
    float b = t+h;

	glColor4ub(255, 255, 255, 255);
	glBegin(GL_QUADS);

		glTexCoord2f(l, t);
		glVertex2f(position.x, position.y);

		glTexCoord2f(l, b);
		glVertex2f(position.x, position.y + size.y);

		glTexCoord2f(r, b);
		glVertex2f(position.x + size.x, position.y + size.y);

		glTexCoord2f(r, t);
		glVertex2f(position.x + size.x, position.y);

	glEnd();
}
