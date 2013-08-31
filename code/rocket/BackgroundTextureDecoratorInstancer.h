//
// Created by Sergei Shubin <s.v.shubin@gmail.com>
//

#ifndef BACKGROUND_TEXTURE_DECORATOR_INSTANCER_H
#define BACKGROUND_TEXTURE_DECORATOR_INSTANCER_H

#include <Rocket/Core/DecoratorInstancer.h>

class BackgroundTextureDecoratorInstancer: public Rocket::Core::DecoratorInstancer {
public:
	// Instances a decorator given the property tag and attributes from the RCSS file.
	// @param[in] name The type of decorator desired.
	// @param[in] properties All RCSS properties associated with the decorator.
	// @return The decorator if it was instanced successful, NULL if an error occured.
	virtual Rocket::Core::Decorator* InstanceDecorator(const Rocket::Core::String& name, const Rocket::Core::PropertyDictionary& properties);
	// Releases the given decorator.
	// @param[in] decorator Decorator to release.
	virtual void ReleaseDecorator(Rocket::Core::Decorator* decorator);

	// Releases the instancer.
	virtual void Release();

	BackgroundTextureDecoratorInstancer();
	virtual ~BackgroundTextureDecoratorInstancer();
};

#endif /* BACKGROUND_TEXTURE_DECORATOR_INSTANCER_H */
