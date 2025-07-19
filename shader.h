#ifndef SHADER_H
#define SHADER_H

#include "common.h"

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include "OpenGLHelper.h"
#include <string>

/*
	@brief:
	Utility class to create, build and run a shader.
		- After creating the shader, call build()
		- You can set static uniforms that don't change every frame by calling shader.use()
		  and then setUniform(), and optionally shader.release()
		- Frame-dynamic uniforms are called the same way, but inside the render loop

	All uniform locations are cached for better performance. They're generally cached upon
	first use, but you can force pre-caching by calling cacheUniform() for each uniform
	after building and activating the shader.

	If you want even more control over the uniforms, you can always do something like:
	Cache the uniform yourself:
		u_ticks = glGetUniformLocation(shader.ID, "ticks");
	Assign the uniform in the render loop:
		glUniform1i(u_ticks, SDL_GetTicks());

 */

using UniformValue = std::variant<
									bool,int,uint32_t,float,
									glm::vec2,glm::ivec2,glm::uvec2,
									glm::vec3,glm::vec4,
									glm::mat2,glm::mat3,glm::mat4
								>;

class Shader
{
public:
    unsigned int ID = 0;
	bool isReady = false;    // Shader is useless until you call build()
	bool isInUse = false;

    // Build from vertex and fragment shaders (could be combined using VERTEX and FRAGMENT #define)
    void build(const char* vertexPath, const char* fragmentPath);
    void _compile(const std::string* pvertexCode, const std::string* pfragmentCode);

	// de/activate the shader
	void use() { glUseProgram(ID); glGetError() == GL_NO_ERROR ? isInUse = true : isInUse = false; };
	void release() { glUseProgram(0); isInUse = false; };

	const std::string GetVertexPath() { return s_vertexPath; }
	const std::string GetFragmentPath() { return s_fragmentPath; }

	void cacheUniform(std::string const& name);
	void setUniform(std::string const& name, UniformValue const& v);

private:
	std::string s_vertexPath;
	std::string s_fragmentPath;
	std::unordered_map<std::string,GLint> uniformCache;
	std::vector<std::string> _uniformNames;
	std::vector<GLint>       _uniformLocs;

    // utility function for checking shader compilation/linking errors.
    void checkCompileErrors(GLuint shader, std::string type);
};
#endif
