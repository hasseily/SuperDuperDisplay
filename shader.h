#ifndef SHADER_H
#define SHADER_H

#include "common.h"

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include "OpenGLHelper.h"
#include <string>

class Shader
{
public:
    unsigned int ID = 0;
    bool isReady = false;
    // Useless until you call build()

    // Build from vertex and fragment shaders (could be combined using VERTEX and FRAGMENT #define)
    void build(const char* vertexPath, const char* fragmentPath);
    void _compile(const std::string* pvertexCode, const std::string* pfragmentCode);
    // activate the shader
    // ------------------------------------------------------------------------
    void use() const;
    // utility uniform functions    (Not all are created. Add as necessary)
    // ------------------------------------------------------------------------
    void setBool(const std::string &name, bool value) const;
    // ------------------------------------------------------------------------
    void setInt(const std::string &name, int value) const;
	void setUInt(const std::string &name, uint32_t value) const;
    // ------------------------------------------------------------------------
    void setFloat(const std::string &name, float value) const;
    // ------------------------------------------------------------------------
    void setVec2(const std::string &name, const glm::vec2 &value) const;
    void setVec2(const std::string &name, float x, float y) const;
	// ------------------------------------------------------------------------
	void setVec2i(const std::string& name, const glm::ivec2& value) const;
    void setVec2i(const std::string& name, int x, int y) const;
    // ------------------------------------------------------------------------
	void setVec2u(const std::string& name, const glm::uvec2& value) const;
    void setVec2u(const std::string& name, unsigned int x, unsigned int y) const;
    // ------------------------------------------------------------------------
    void setVec3(const std::string &name, const glm::vec3 &value) const;
    void setVec3(const std::string &name, float x, float y, float z) const;
    // ------------------------------------------------------------------------
    void setVec4(const std::string &name, const glm::vec4 &value) const;
    void setVec4(const std::string &name, float x, float y, float z, float w) const;
    // ------------------------------------------------------------------------
    void setMat2(const std::string &name, const glm::mat2 &mat) const;
    // ------------------------------------------------------------------------
    void setMat3(const std::string &name, const glm::mat3 &mat) const;
    // ------------------------------------------------------------------------
    void setMat4(const std::string &name, const glm::mat4 &mat) const;
	
	const std::string GetVertexPath() { return s_vertexPath; }
	const std::string GetFragmentPath() { return s_fragmentPath; }

private:
	std::string s_vertexPath;
	std::string s_fragmentPath;
    // utility function for checking shader compilation/linking errors.
    // ------------------------------------------------------------------------
    void checkCompileErrors(GLuint shader, std::string type);
};
#endif
