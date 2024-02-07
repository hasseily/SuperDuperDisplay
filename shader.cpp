#include "shader.h"


void Shader::build(const char* vertexPath, const char* fragmentPath)
{
	s_vertexPath = std::string(vertexPath);
	s_fragmentPath = std::string(fragmentPath);
	// 1. retrieve the vertex/fragment source code from filePath
	std::string vertexCode;
	std::string fragmentCode;
	std::ifstream vShaderFile;
	std::ifstream fShaderFile;
	// ensure ifstream objects can throw exceptions:
	vShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
	fShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
	try
	{
		// open files
		vShaderFile.open(vertexPath);
		fShaderFile.open(fragmentPath);
		std::stringstream vShaderStream, fShaderStream;
		// read file's buffer contents into streams
		vShaderStream << vShaderFile.rdbuf();
		fShaderStream << fShaderFile.rdbuf();
		// close file handlers
		vShaderFile.close();
		fShaderFile.close();
		// convert stream into string
		auto glhelper = OpenGLHelper::GetInstance();
		vertexCode += *glhelper->get_glsl_version() + std::string("\n#define VERTEX\n") + vShaderStream.str();
		fragmentCode += *glhelper->get_glsl_version() + std::string("\n#define FRAGMENT\n") + fShaderStream.str();
		// std::cout << fragmentCode << std::endl;
	}
	catch (std::ifstream::failure& e)
	{
		std::cout << "ERROR::SHADER::FILE_NOT_SUCCESSFULLY_READ: " << e.what() << std::endl;
		exit(1);
	}
	_compile(&vertexCode, &fragmentCode);
}

void Shader::_compile(const std::string* pvertexCode, const std::string* pfragmentCode)
{
	const char* vShaderCode = pvertexCode->c_str();
	const char* fShaderCode = pfragmentCode->c_str();
	// 2. compile shaders
	unsigned int vertex, fragment;
	// vertex shader
	vertex = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertex, 1, &vShaderCode, NULL);
	glCompileShader(vertex);
	checkCompileErrors(vertex, "VERTEX");
	// fragment Shader
	fragment = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragment, 1, &fShaderCode, NULL);
	glCompileShader(fragment);
	checkCompileErrors(fragment, "FRAGMENT");
	// shader Program
	ID = glCreateProgram();
	glAttachShader(ID, vertex);
	glAttachShader(ID, fragment);
	glLinkProgram(ID);
	checkCompileErrors(ID, "PROGRAM");
	// delete the shaders as they're linked into our program now and no longer necessary
	glDeleteShader(vertex);
	glDeleteShader(fragment);
	isReady = true;
}

void Shader::use() const
{

	glUseProgram(ID);
}

void Shader::setBool(const std::string& name, bool value) const
{

	glUniform1i(glGetUniformLocation(ID, name.c_str()), (int)value);
}

void Shader::setInt(const std::string& name, int value) const
{

	glUniform1i(glGetUniformLocation(ID, name.c_str()), value);
}

void Shader::setUInt(const std::string& name, uint32_t value) const
{
	
	glUniform1ui(glGetUniformLocation(ID, name.c_str()), value);
}

void Shader::setFloat(const std::string& name, float value) const
{

	glUniform1f(glGetUniformLocation(ID, name.c_str()), value);
}

void Shader::setVec2(const std::string& name, float x, float y) const
{

	glUniform2f(glGetUniformLocation(ID, name.c_str()), x, y);
}

void Shader::setVec2(const std::string& name, const glm::vec2& value) const
{

	glUniform2fv(glGetUniformLocation(ID, name.c_str()), 1, &value[0]);
}

void Shader::setVec2u(const std::string& name, unsigned int x, unsigned int y) const
{
	glUniform2ui(glGetUniformLocation(ID, name.c_str()), x, y);
}

void Shader::setVec2u(const std::string& name, const glm::uvec2& value) const
{
	glUniform2uiv(glGetUniformLocation(ID, name.c_str()), 1, &value[0]);
}

void Shader::setVec3(const std::string& name, float x, float y, float z) const
{

	glUniform3f(glGetUniformLocation(ID, name.c_str()), x, y, z);
}

void Shader::setVec3(const std::string& name, const glm::vec3& value) const
{

	glUniform3fv(glGetUniformLocation(ID, name.c_str()), 1, &value[0]);
}

void Shader::setVec4(const std::string& name, float x, float y, float z, float w) const
{

	glUniform4f(glGetUniformLocation(ID, name.c_str()), x, y, z, w);
}

void Shader::setVec4(const std::string& name, const glm::vec4& value) const
{

	glUniform4fv(glGetUniformLocation(ID, name.c_str()), 1, &value[0]);
}

void Shader::setMat2(const std::string& name, const glm::mat2& mat) const
{
	glUniformMatrix2fv(glGetUniformLocation(ID, name.c_str()), 1, GL_FALSE, &mat[0][0]);
}

void Shader::setMat3(const std::string& name, const glm::mat3& mat) const
{
	glUniformMatrix3fv(glGetUniformLocation(ID, name.c_str()), 1, GL_FALSE, &mat[0][0]);
}

void Shader::setMat4(const std::string& name, const glm::mat4& mat) const
{
	glUniformMatrix4fv(glGetUniformLocation(ID, name.c_str()), 1, GL_FALSE, &mat[0][0]);
}

void Shader::checkCompileErrors(GLuint shader, std::string type)
{
	GLint success;
	GLchar infoLog[1024];
	if (type != "PROGRAM")
	{
		glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
		if (!success)
		{
			glGetShaderInfoLog(shader, 1024, NULL, infoLog);
			std::cout << "ERROR::SHADER_COMPILATION_ERROR of type: " << type << "\n" << infoLog << "\n -- --------------------------------------------------- -- " << std::endl;
			std::cout << "   Vertex:   " << s_vertexPath << std::endl;
			std::cout << "   Fragment: " << s_fragmentPath << std::endl;
			exit(1);
		}
	}
	else
	{
		glGetProgramiv(shader, GL_LINK_STATUS, &success);
		if (!success)
		{
			glGetProgramInfoLog(shader, 1024, NULL, infoLog);
			std::cout << "ERROR::PROGRAM_LINKING_ERROR of type: " << type << "\n" << infoLog << "\n -- --------------------------------------------------- -- " << std::endl;
			std::cout << "   Vertex:   " << s_vertexPath << std::endl;
			std::cout << "   Fragment: " << s_fragmentPath << std::endl;
			exit(1);
		}
	}
}
