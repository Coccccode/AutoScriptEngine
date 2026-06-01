#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include "OcrApi.h"
#include <iostream>
#include <sstream>
#include <vector>
#include <fstream>
#include <cstdio>

static const char* BASE64_CHARS =
"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
"abcdefghijklmnopqrstuvwxyz"
"0123456789+/";

OcrApi::OcrApi(int port, const std::string& configName)
	: m_port(port)
	, m_configName(configName)
	, m_processHandle(NULL)
	, m_running(false)
{
}

OcrApi::~OcrApi()
{
	if (m_processHandle)
	{
		TerminateProcess(m_processHandle, 0);
		CloseHandle(m_processHandle);
		m_processHandle = NULL;
	}
}

bool OcrApi::start()
{
	STARTUPINFOA si;
	PROCESS_INFORMATION pi;
	ZeroMemory(&si, sizeof(si));
	ZeroMemory(&pi, sizeof(pi));
	si.cb = sizeof(si);

	std::string cmdLine = "PaddleOCR-json.exe -port=" + std::to_string(m_port);
	if (!m_configName.empty())
	{
		cmdLine += " --config_path=models/" + m_configName;
	}

	char currentDir[MAX_PATH];
	GetCurrentDirectoryA(MAX_PATH, currentDir);
	std::string ocrDir = std::string(currentDir) + "\\vendor\\PaddleOCR-json_v.1.3.1";
	DWORD attrs = GetFileAttributesA(ocrDir.c_str());
	if (attrs == INVALID_FILE_ATTRIBUTES || !(attrs & FILE_ATTRIBUTE_DIRECTORY))
	{
		ocrDir = std::string(currentDir) + "\\PaddleOCR-json_v.1.3.1";
	}

	if (!SetCurrentDirectoryA(ocrDir.c_str()))
	{
		std::cerr << "failed to switch to PaddleOCR directory: " << ocrDir << std::endl;
		return false;
	}

	std::vector<char> cmdBuffer(cmdLine.begin(), cmdLine.end());
	cmdBuffer.push_back('\0');

	if (!CreateProcessA(
		NULL,
		cmdBuffer.data(),
		NULL,
		NULL,
		FALSE,
		CREATE_NO_WINDOW,
		NULL,
		NULL,
		&si,
		&pi))
	{
		std::cerr << "failed to start PaddleOCR-json.exe, error: " << GetLastError() << std::endl;
		SetCurrentDirectoryA(currentDir);
		return false;
	}

	SetCurrentDirectoryA(currentDir);

	m_processHandle = pi.hProcess;
	CloseHandle(pi.hThread);

	m_running = true;
	for (int retry = 0; retry < 50; retry++)
	{
		if (!isRunning())
		{
			std::cerr << "PaddleOCR-json exited after start" << std::endl;
			return false;
		}
		if (canConnect())
		{
			std::cout << "PaddleOCR-json started, port: " << m_port << std::endl;
			return true;
		}
		Sleep(200);
	}

	std::cerr << "PaddleOCR-json start timeout, port not listening: " << m_port << std::endl;
	return false;
}

bool OcrApi::isRunning()
{
	if (!m_processHandle)
		return false;

	DWORD exitCode;
	if (GetExitCodeProcess(m_processHandle, &exitCode))
	{
		if (exitCode != STILL_ACTIVE)
		{
			m_running = false;
			return false;
		}
	}
	return m_running;
}

std::vector<OcrResult> OcrApi::recognize(const cv::Mat& image)
{
	return recognize(image, m_configName);
}

std::vector<OcrResult> OcrApi::recognize(const cv::Mat& image, const std::string& configName)
{
	if (!isRunning())
	{
		std::cerr << "OCR service is not running" << std::endl;
		return {};
	}

	std::string imagePath = matToTempImage(image);
	if (imagePath.empty())
	{
		std::cerr << "failed to save OCR temporary image" << std::endl;
		return {};
	}

	for (char& c : imagePath)
	{
		if (c == '\\') c = '/';
	}

	Json::Value request;
	request["image_path"] = imagePath;

	Json::FastWriter writer;
	std::string body = writer.write(request);

	std::string response = sendSocketRequest("127.0.0.1", m_port, body);
	std::remove(imagePath.c_str());

	if (response.empty())
	{
		std::cerr << "OCR socket request failed" << std::endl;
		return {};
	}

	return parseOcrResponse(response);
}
std::string OcrApi::findText(const cv::Mat& image, const std::string& target)
{
	auto results = recognize(image);
	for (const auto& r : results)
	{
		if (r.text.find(target) != std::string::npos)
		{
			return r.text;
		}
	}
	return "";
}

std::string OcrApi::matToBase64(const cv::Mat& image)
{
	std::vector<unsigned char> buf;
	std::vector<int> params;
	params.push_back(cv::IMWRITE_JPEG_QUALITY);
	params.push_back(95);
	if (!cv::imencode(".png", image, buf, params))
	{
		return "";
	}
	return base64Encode(buf.data(), buf.size());
}

std::string OcrApi::base64Encode(const unsigned char* data, size_t len)
{
	std::string result;
	result.reserve(((len + 2) / 3) * 4);

	for (size_t i = 0; i < len; i += 3)
	{
		unsigned long n = (unsigned long)(data[i]) << 16;
		if (i + 1 < len) n |= (unsigned long)(data[i + 1]) << 8;
		if (i + 2 < len) n |= (unsigned long)(data[i + 2]);

		result += BASE64_CHARS[(n >> 18) & 0x3F];
		result += BASE64_CHARS[(n >> 12) & 0x3F];
		result += (i + 1 < len) ? BASE64_CHARS[(n >> 6) & 0x3F] : '=';
		result += (i + 2 < len) ? BASE64_CHARS[n & 0x3F] : '=';
	}
	return result;
}

std::string OcrApi::matToTempImage(const cv::Mat& image)
{
	if (image.empty()) return "";

	char tempDir[MAX_PATH] = { 0 };
	char tempFile[MAX_PATH] = { 0 };
	if (GetTempPathA(MAX_PATH, tempDir) == 0)
	{
		return "";
	}
	if (GetTempFileNameA(tempDir, "ocr", 0, tempFile) == 0)
	{
		return "";
	}

	std::string imagePath = std::string(tempFile) + ".png";
	DeleteFileA(tempFile);
	if (!cv::imwrite(imagePath, image))
	{
		return "";
	}
	return imagePath;
}

std::string OcrApi::sendSocketRequest(const std::string& host, int port, const std::string& body)
{
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		std::cerr << "WSAStartup failed" << std::endl;
		return "";
	}

	SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock == INVALID_SOCKET)
	{
		WSACleanup();
		return "";
	}

	DWORD timeout = 10000;
	setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
	setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout));

	sockaddr_in addr;
	ZeroMemory(&addr, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons((u_short)port);
	inet_pton(AF_INET, host.c_str(), &addr.sin_addr);

	if (connect(sock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR)
	{
		std::cerr << "OCR socket connect failed: " << WSAGetLastError() << std::endl;
		closesocket(sock);
		WSACleanup();
		return "";
	}

	std::string request = body;
	if (request.empty() || request.back() != '\n')
	{
		request += "\n";
	}

	int sent = send(sock, request.c_str(), (int)request.size(), 0);
	if (sent == SOCKET_ERROR)
	{
		std::cerr << "OCR socket send failed: " << WSAGetLastError() << std::endl;
		closesocket(sock);
		WSACleanup();
		return "";
	}

	std::string response;
	char buffer[8192];
	int jsonDepth = 0;
	bool jsonStarted = false;
	bool inString = false;
	bool escaped = false;
	while (true)
	{
		int received = recv(sock, buffer, sizeof(buffer), 0);
		if (received <= 0) break;

		response.append(buffer, received);
		for (int i = 0; i < received; i++)
		{
			char ch = buffer[i];
			if (escaped)
			{
				escaped = false;
				continue;
			}
			if (ch == '\\' && inString)
			{
				escaped = true;
				continue;
			}
			if (ch == '"')
			{
				inString = !inString;
				continue;
			}
			if (inString) continue;

			if (ch == '{')
			{
				jsonStarted = true;
				jsonDepth++;
			}
			else if (ch == '}' && jsonStarted)
			{
				jsonDepth--;
			}
		}

		if (jsonStarted && jsonDepth <= 0) break;
	}

	closesocket(sock);
	WSACleanup();
	return response;
}

bool OcrApi::canConnect()
{
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		return false;
	}

	SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock == INVALID_SOCKET)
	{
		WSACleanup();
		return false;
	}

	sockaddr_in addr;
	ZeroMemory(&addr, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons((u_short)m_port);
	inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

	bool ok = connect(sock, (sockaddr*)&addr, sizeof(addr)) != SOCKET_ERROR;
	closesocket(sock);
	WSACleanup();
	return ok;
}
std::vector<OcrResult> OcrApi::parseOcrResponse(const std::string& jsonStr)
{
	std::vector<OcrResult> results;

	Json::Reader reader;
	Json::Value root;
	if (!reader.parse(jsonStr, root))
	{
		std::cerr << "failed to parse OCR response: " << reader.getFormattedErrorMessages() << std::endl;
		return results;
	}

	if (!root.isMember("data"))
	{
		std::cerr << "OCR response missing data field" << std::endl;
		return results;
	}

	const Json::Value& data = root["data"];
	for (Json::ArrayIndex i = 0; i < data.size(); i++)
	{
		const Json::Value& item = data[i];
		OcrResult result;
		result.text = item["text"].asString();
		result.score = item.get("score", 0.0).asDouble();

		if (item.isMember("box"))
		{
			const Json::Value& box = item["box"];
			if (box.size() >= 4 && box[0].isArray())
			{
				int x1 = box[0][0].asInt();
				int y1 = box[0][1].asInt();
				int x2 = box[2][0].asInt();
				int y2 = box[2][1].asInt();
				result.x = x1;
				result.y = y1;
				result.width = x2 - x1;
				result.height = y2 - y1;
			}
			else if (box.size() >= 4)
			{
				result.x = box[0].asInt();
				result.y = box[1].asInt();
				result.width = box[2].asInt() - result.x;
				result.height = box[3].asInt() - result.y;
			}
		}
		else
		{
			result.x = item.get("x", 0).asInt();
			result.y = item.get("y", 0).asInt();
			result.width = item.get("width", 0).asInt();
			result.height = item.get("height", 0).asInt();
		}
		results.push_back(result);
	}

	return results;
}
