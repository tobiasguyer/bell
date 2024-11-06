#pragma once

#include <stddef.h>     // for size_t
#include <cstdint>      // for uint8_t, int32_t
#include <memory>       // for make_unique, unique_ptr
#include <string>       // for string
#include <string_view>  // for string_view
#include <utility>      // for pair
#include <vector>       // for vector

#include "SocketStream.h"  // for SocketStream
#include "URLParser.h"     // for URLParser
#ifndef BELL_DISABLE_FMT
#include "fmt/core.h"  // for format
#endif
#include "picohttpparser.h"  // for phr_header

namespace bell {

class HTTPClient {
 public:
  // most basic header type, represents by a key-val
  typedef std::pair<std::string, std::string> ValueHeader;

  typedef std::vector<ValueHeader> Headers;

  // Helper over ValueHeader, formatting a HTTP bytes range
  struct RangeHeader {
    static ValueHeader range(int32_t from, int32_t to) {
#ifndef BELL_DISABLE_FMT
      return ValueHeader{"Range", fmt::format("bytes={}-{}", from, to)};
#else
      return ValueHeader{
          "Range", "bytes=" + std::to_string(from) + "-" + std::to_string(to)};
#endif
    }

    static ValueHeader last(int32_t nbytes) {
#ifndef BELL_DISABLE_FMT
      return ValueHeader{"Range", fmt::format("bytes=-{}", nbytes)};
#else
      return ValueHeader{"Range", "bytes=-" + std::to_string(nbytes)};
#endif
    }
  };

  class Response {
   public:
    Response(size_t numHeaders = 32) 
      : phResponseHeaders(new phr_header[numHeaders]), maxHeaders(numHeaders) {}
    ~Response();

    /**
    * Initializes a connection with a given url.
    */
    int connect(const std::string& url, size_t numHeaders = 32);
    int reconnect();

    bool rawRequest(const std::string& method, const std::string& url,
                    const std::vector<uint8_t>& content, Headers& headers);
    bool get(const std::string& url, Headers headers = {});
    bool post(const std::string& url, Headers headers = {},
              const std::vector<uint8_t>& body = {});

    std::string_view body();
    std::vector<uint8_t> bytes();

    std::string_view header(const std::string& headerName);
    bell::SocketStream& stream() { return this->socketStream; }

    size_t contentLength();
    size_t totalLength();

   private:
    bell::URLParser urlParser;
    bell::SocketStream socketStream;

    phr_header* phResponseHeaders;
    size_t maxHeaders;  // Store max headers to handle numHeaders limit
    const size_t HTTP_BUF_SIZE = 1024;

    std::vector<uint8_t> httpBuffer = std::vector<uint8_t>(HTTP_BUF_SIZE);
    std::vector<uint8_t> rawBody = std::vector<uint8_t>();
    size_t httpBufferAvailable;

    size_t contentSize = 0;
    bool hasContentSize = false;

    Headers responseHeaders;

    bool readResponseHeaders();
    void readRawBody();
  };

  enum class Method : uint8_t { GET = 0, POST = 1 };

  struct Request {
    std::string url;
    Method method;
    Headers headers;
  };

  static std::unique_ptr<Response> get(const std::string& url,
                                       Headers headers = {}, size_t numHeaders = 32) {
    auto response = std::make_unique<Response>(numHeaders);
    if(response->connect(url, numHeaders) == 0)
      response->get(url, headers);
    return response;
  }

  static std::unique_ptr<Response> post(const std::string& url,
                                        Headers headers = {},
                                        const std::vector<uint8_t>& body = {}, 
                                        size_t numHeaders = 32) {
    auto response = std::make_unique<Response>(numHeaders);
    if(response->connect(url, numHeaders) == 0)
      response->post(url, headers, body);
    return response;
  }
};

}  // namespace bell