//
// Created by Sergei Shubin <s.v.shubin@gmail.com>
//

#include "BackgroundTextureDecoratorInstancer.h"
#include "BackgroundTextureDecorator.h"
#include <math.h>

Rocket::Core::Decorator* BackgroundTextureDecoratorInstancer::InstanceDecorator(const Rocket::Core::String& name, const Rocket::Core::PropertyDictionary& properties) {
	const Rocket::Core::Property *image_source_property = properties.GetProperty("image-src");
	const Rocket::Core::Property *offset_x = properties.GetProperty("offset-x");
	const Rocket::Core::Property *offset_y = properties.GetProperty("offset-x");
    const Rocket::Core::Property *p_wrap_s = properties.GetProperty("wrap-s");
    const Rocket::Core::Property *p_wrap_t = properties.GetProperty("wrap-t");

	Rocket::Core::String image_source = image_source_property->Get< Rocket::Core::String >();
    
    GLint wrap_s = p_wrap_s->Get<Rocket::Core::String>() == "clamp" ? GL_CLAMP : GL_REPEAT;
    GLint wrap_t = p_wrap_t->Get<Rocket::Core::String>() == "clamp" ? GL_CLAMP : GL_REPEAT;

	BackgroundTextureDecorator *decorator = new BackgroundTextureDecorator();
	if (decorator->Initialise(image_source, image_source_property->source, wrap_s, wrap_t)) {
		return decorator;
	}
	decorator->RemoveReference();
	ReleaseDecorator(decorator);
	return NULL;
}

void BackgroundTextureDecoratorInstancer::ReleaseDecorator(Rocket::Core::Decorator* decorator) {
	delete decorator;
}

void BackgroundTextureDecoratorInstancer::Release() {
	delete this;
}

BackgroundTextureDecoratorInstancer::BackgroundTextureDecoratorInstancer() {
	RegisterProperty("image-src", "").AddParser("string");
	RegisterProperty("offset-x", "0.0").AddParser("number");
	RegisterProperty("offset-y", "0.0").AddParser("number");
	RegisterProperty("wrap-s", "repeat").AddParser("string");
	RegisterProperty("wrap-t", "repeat").AddParser("string");
}

BackgroundTextureDecoratorInstancer::~BackgroundTextureDecoratorInstancer() {
}
