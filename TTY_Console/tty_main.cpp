#include <iostream>

#include <Windows.h>

int main(int argc, char* argv[]) {
	if (argc != 2)
		exit(1);

	char* pipe_name = argv[1];

	if (pipe_name == nullptr)
		exit(1);

	//std::cin.get();

	HANDLE pipe_handle = CreateFileA(
		pipe_name,
		GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		INVALID_HANDLE_VALUE
	);

	if (pipe_handle == INVALID_HANDLE_VALUE)
		return false;

	std::unique_ptr<char[]> buf{ new char[1024] };

	for (;;) {
		DWORD total_bytes_read{};

		auto res = ReadFile(pipe_handle, buf.get(),
			1024, &total_bytes_read,
			NULL);

		if (!res)
			break;

		std::string data{ buf.get(), buf.get() + total_bytes_read };

		std::cout << data;
		std::cout.flush();
	}

	CloseHandle(pipe_handle);
}