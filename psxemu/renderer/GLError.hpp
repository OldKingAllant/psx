#pragma once

/*
  Enum used to represent underlying OpenGL errors
  and also our errors
* GL_NO_ERROR
* GL_INVALID_ENUM
* GL_INVALID_VALUE
* GL_INVALID_OPERATION
* GL_INVALID_FRAMEBUFFER_OPERATION
* GL_OUT_OF_MEMORY
* GL_STACK_UNDERFLOW
* GL_STACK_OVERFLOW
*/
enum class GLError {
	OK = 0,                        // Autoesplicative
	INVALID_ENUM,                  // An enumerated argument (such as the format of a texture) is not valid
	INVALID_VALUE,                 // Numeric value (e.g. size of a texture) is out of range
	INVALID_OPERATION,             // May be a lot of different things
	INVALID_FRAMEBUFFER_OPERATION, // Attempting to use a framebuffer in an invalid state
	OUT_OF_MEMORY,				   // Autoesplicative
	STACK_UNDERFLOW,               // Autoesplicative
	STACK_OVERFLOW,                // Autoesplicative
	MAP_FAILED_BUFFER_OVERFLOW,    // Attempted to map a buffer outside its range
	MAP_FAILED_INVALID_PROTS,      // Attempted to map a buffer with invalid protections
	UNKNOWN,                       // IDK
	READ_FAILED_BUFFER_MAPPED,     // Attempting GL read buffer while the buffer is mapped without the "map persistent" bit
	READ_FAILED_BUFFER_OVERFLOW,   // Autoesplicative
	WRITE_FAILED_BUFFER_MAPPED,	   // Same as for buffer read
	WRITE_FAILED_BUFFER_OVERFLOW,  // Same as for buffer read
	RESIZE_FAILED_BUFFER_MAPPED,   // Buffer was CPU mapped while resizing
	PUSH_FAILED_INVALID_BUFFER,    // Attempting to push data to invalid buffer inside a vertex buffer
	PUSH_FAILED_TYPE_MISMATCH,     // Attempting to push data to buffer inside a vertex buffer with an incorrect type
	PUSH_FAILED_SIZE_MISMATCH,     // Attempting to push data to buffer inside a vertex buffer with an incorrect size
	PUSH_FAILED_BUFFER_OVERFLOW,   // Buffer overflow while appending to vertex buffer
	COPY_FAILED_BUFFER_MAPPED,     // Attempting to copy a buffer to another while one of them was mapped or they do not have the PERSISTENT flag set
	CLEAR_FAILED_INVALID_BUFFER,   // 
	RESIZE_FAILED_MISSING_BUFFERS, // Attempting to resize vertex buffer without specifying all sub-buffers
	UNIFORM_NOT_FOUND,             // glGetUniformLocation returned -1
	INVALID_TEXTURE_FORMAT,
	INVALID_TEXTURE_ACCESS,
	INVALID_PIXEL_FORMAT,
	INVALID_PIXEL_DATATYPE,
	INVALID_SAMPLE_FILTER,
	INVALID_ATTACHMENT,
	FRAMEBUFFER_INCOMPLETE
};