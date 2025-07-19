#include "shader.h"

// compile‑time dispatch for glUniform*
template<typename T> inline constexpr bool always_false_v = false;

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

void Shader::cacheUniform(std::string const& name) {
	uniformCache[name] = glGetUniformLocation(ID, name.c_str());
}

void Shader::setUniform(std::string const& name, UniformValue const& v) {
	if (!isInUse) {
		this->use();
		if (!isInUse)
			{
				std::cerr << "Shader error: Calling glUseProgram() returns an error" << std::endl;
				return;
			}
	}

	GLint loc = -1;
	auto it = uniformCache.find(name);
	if (it == uniformCache.end()) {
		loc = glGetUniformLocation(ID, name.c_str());
		uniformCache[name] = loc;
		if (loc < 0)		// uniform not available in shader
		{
			std::cout << "Uniform " << name << " not available in shader" << std::endl;
			return;
		}
	} else {
		loc = it->second;
	}

	std::visit([&](auto const& arg){
		using T = std::decay_t<decltype(arg)>;
		if constexpr (std::is_same_v<T,bool>) {
			glUniform1i(loc, arg ? 1 : 0);
		}
		else if constexpr (std::is_same_v<T,int>) {
			glUniform1i(loc, arg);
		}
		else if constexpr (std::is_same_v<T,uint32_t>) {
			glUniform1ui(loc, arg);
		}
		else if constexpr (std::is_same_v<T,float>) {
			glUniform1f(loc, arg);
		}
		else if constexpr (std::is_same_v<T,glm::vec2>) {
			glUniform2fv(loc, 1, &arg.x);
		}
		else if constexpr (std::is_same_v<T,glm::ivec2>) {
			glUniform2iv(loc, 1, &arg.x);
		}
		else if constexpr (std::is_same_v<T,glm::uvec2>) {
			glUniform2uiv(loc, 1, &arg.x);
		}
		else if constexpr (std::is_same_v<T,glm::vec3>) {
			glUniform3fv(loc, 1, &arg.x);
		}
		else if constexpr (std::is_same_v<T,glm::vec4>) {
			glUniform4fv(loc, 1, &arg.x);
		}
		else if constexpr (std::is_same_v<T,glm::mat2>) {
			glUniformMatrix2fv(loc, 1, GL_FALSE, &arg[0][0]);
		}
		else if constexpr (std::is_same_v<T,glm::mat3>) {
			glUniformMatrix3fv(loc, 1, GL_FALSE, &arg[0][0]);
		}
		else if constexpr (std::is_same_v<T,glm::mat4>) {
			glUniformMatrix4fv(loc, 1, GL_FALSE, &arg[0][0]);
		}
		else {
			static_assert(always_false_v<T>, "Unsupported uniform type");
		}
#ifdef DEBUG
		if ((glGetError()) != GL_NO_ERROR) {
			std::cerr << "Set Uniform error for: "<< name << std::endl;
		}
#endif
	}, v);
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
