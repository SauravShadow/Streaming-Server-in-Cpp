#include "streaming/video_handler.h"
#include "streaming/file_streamer.h"
#include "streaming/range_parser.h"

#include <cctype>
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <sstream>
#include <string>
#include <vector>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

namespace streaming {

namespace {

std::string decodeUrlPath(const std::string &path) {
  std::string decoded;
  decoded.reserve(path.size());

  for (size_t i = 0; i < path.size(); ++i) {
    if (path[i] == '%' && i + 2 < path.size() && std::isxdigit(path[i + 1]) &&
        std::isxdigit(path[i + 2])) {
      std::string hex = path.substr(i + 1, 2);
      decoded.push_back(static_cast<char>(std::stoi(hex, nullptr, 16)));
      i += 2;
    } else if (path[i] == '+') {
      decoded.push_back(' ');
    } else {
      decoded.push_back(path[i]);
    }
  }

  return decoded;
}

std::string encodeUrlPath(const std::string &path) {
  std::ostringstream encoded;

  for (unsigned char ch : path) {
    if (std::isalnum(ch) || ch == '/' || ch == '-' || ch == '_' || ch == '.' ||
        ch == '~') {
      encoded << ch;
    } else {
      const char *digits = "0123456789ABCDEF";
      encoded << '%' << digits[ch >> 4] << digits[ch & 15];
    }
  }

  return encoded.str();
}

std::string escapeHtml(const std::string &text) {
  std::string escaped;

  for (char ch : text) {
    switch (ch) {
    case '&':
      escaped += "&amp;";
      break;
    case '<':
      escaped += "&lt;";
      break;
    case '>':
      escaped += "&gt;";
      break;
    case '"':
      escaped += "&quot;";
      break;
    default:
      escaped.push_back(ch);
      break;
    }
  }

  return escaped;
}

std::string getContentType(const std::string &file_path) {
  std::filesystem::path path(file_path);
  std::string ext = path.extension().string();

  for (char &ch : ext) {
    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  }

  if (ext == ".mp4")
    return "video/mp4";
  if (ext == ".mkv")
    return "video/x-matroska";
  if (ext == ".webm")
    return "video/webm";
  if (ext == ".mov")
    return "video/quicktime";
  if (ext == ".avi")
    return "video/x-msvideo";
  if (ext == ".webp")
    return "image/webp";
  if (ext == ".jpg" || ext == ".jpeg" || ext == ".jfif")
    return "image/jpeg";
  if (ext == ".png")
    return "image/png";
  if (ext == ".avif")
    return "image/avif";

  return "application/octet-stream";
}

bool sendAll(int client_fd, const std::string &data) {
  size_t total_sent = 0;

  while (total_sent < data.size()) {
    ssize_t sent =
        send(client_fd, data.data() + total_sent, data.size() - total_sent, 0);
    if (sent <= 0)
      return false;

    total_sent += sent;
  }

  return true;
}

std::string resolveVideoPath(const std::string &request_path) {
  const std::vector<std::string> candidates = {
      "." + request_path, ".." + request_path, "../.." + request_path,
      "../../.." + request_path};

  for (const std::string &candidate : candidates) {
    struct stat st;
    if (stat(candidate.c_str(), &st) == 0)
      return candidate;
  }

  return "." + request_path;
}

bool isDirectory(const std::string &file_path) {
  struct stat st;
  return stat(file_path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

bool isVideoFile(const std::filesystem::path &path) {
  std::string ext = path.extension().string();

  for (char &ch : ext) {
    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  }

  return ext == ".mp4" || ext == ".mkv" || ext == ".webm" || ext == ".mov" ||
         ext == ".avi";
}

bool isImageFile(const std::filesystem::path &path) {
  std::string ext = path.extension().string();

  for (char &ch : ext) {
    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  }

  return ext == ".webp" || ext == ".jpg" || ext == ".jpeg" || ext == ".png" ||
         ext == ".jfif" || ext == ".avif";
}

std::string displayTitle(const std::string &name) {
  std::string title = name;
  std::replace(title.begin(), title.end(), '_', ' ');
  return title;
}

std::string getSeriesDescription(const std::string &series_name) {
  if (series_name == "Black Lagoon") {
    return "Welcome to Roanapur-where bullets talk louder than morals and "
           "survival is the only law. Watch Black Lagoon if you're ready to "
           "dive into chaos, crime, and characters who don't just cross the "
           "line-they erase it.";
  }

  return "A growing shelf from your local library. Drop episodes anywhere "
         "inside the video folder and they will appear here automatically.";
}

std::string findCoverForSeries(const std::filesystem::path &series_path) {
  if (!std::filesystem::exists(series_path))
    return "";

  for (const auto &entry : std::filesystem::recursive_directory_iterator(
           series_path,
           std::filesystem::directory_options::skip_permission_denied)) {
    if (entry.is_regular_file() && isImageFile(entry.path())) {
      return entry.path().string();
    }
  }

  return "";
}

std::string makeVideoUrl(const std::string &request_path,
                         const std::string &relative_path) {
  std::string href = request_path;

  if (!href.empty() && href.back() != '/')
    href += "/";

  href += encodeUrlPath(relative_path);
  return href;
}

std::string makeAssetUrl(const std::string &request_path,
                         const std::filesystem::path &directory_path,
                         const std::filesystem::path &asset_path) {
  std::string relative_path =
      std::filesystem::relative(asset_path, directory_path).string();
  return makeVideoUrl(request_path, relative_path);
}

std::string getTopLevelFolder(const std::filesystem::path &relative_path) {
  auto it = relative_path.begin();
  if (it == relative_path.end())
    return "Library";

  return it->string();
}

struct SeriesSection {
  std::string name;
  std::string cover_url;
  std::vector<std::filesystem::path> videos;
};

std::string buildLibraryPage(const http::HttpRequest &req,
                             const std::string &directory_path) {
  std::vector<std::filesystem::path> video_paths;

  for (const auto &entry : std::filesystem::recursive_directory_iterator(
           directory_path,
           std::filesystem::directory_options::skip_permission_denied)) {
    if (entry.is_regular_file() && isVideoFile(entry.path())) {
      video_paths.push_back(entry.path());
    }
  }

  std::sort(video_paths.begin(), video_paths.end(),
            [directory_path](const std::filesystem::path &left,
                             const std::filesystem::path &right) {
              return std::filesystem::relative(left, directory_path).string() <
                     std::filesystem::relative(right, directory_path).string();
            });

  std::vector<SeriesSection> sections;

  for (const auto &video_path : video_paths) {
    std::filesystem::path relative_path =
        std::filesystem::relative(video_path, directory_path);
    std::string series_name = getTopLevelFolder(relative_path);

    auto existing = std::find_if(
        sections.begin(), sections.end(),
        [&series_name](const SeriesSection &section) {
          return section.name == series_name;
        });

    if (existing == sections.end()) {
      SeriesSection section;
      section.name = series_name;

      std::filesystem::path cover_path =
          findCoverForSeries(std::filesystem::path(directory_path) / series_name);
      if (!cover_path.empty()) {
        section.cover_url = makeAssetUrl(req.path, directory_path, cover_path);
      }

      section.videos.push_back(relative_path);
      sections.push_back(section);
    } else {
      existing->videos.push_back(relative_path);
    }
  }

  std::ostringstream body;
  body << R"(<!doctype html><html><head><meta charset="utf-8">)"
       << R"(<meta name="viewport" content="width=device-width, initial-scale=1">)"
       << R"(<title>Streaming Library</title><style>)"
       << R"(:root{color-scheme:dark;--bg:#0b0b0b;--panel:#171717;--panel-2:#202020;--text:#f3efe6;--muted:#b7aea2;--line:#3a342d;--accent:#e3483b;--accent-2:#65c18c;}*{box-sizing:border-box}body{margin:0;font-family:Inter,Arial,sans-serif;background:var(--bg);color:var(--text);}a{color:inherit}.shell{min-height:100vh;background:linear-gradient(180deg,rgba(227,72,59,.16),rgba(11,11,11,.2) 360px),#0b0b0b}.topbar{display:flex;justify-content:space-between;gap:16px;align-items:center;padding:20px clamp(16px,4%,56px);border-bottom:1px solid var(--line);background:rgba(11,11,11,.92);position:sticky;top:0;z-index:10}.brand{font-weight:800;letter-spacing:0;text-transform:uppercase}.navlink{border:1px solid var(--line);padding:10px 14px;border-radius:8px;text-decoration:none;color:var(--muted)}.hero{display:grid;grid-template-columns:minmax(0,1.1fr) minmax(260px,.9fr);gap:32px;align-items:end;padding:48px clamp(16px,4%,56px) 28px}.eyebrow{color:var(--accent-2);font-weight:700;text-transform:uppercase;font-size:13px;letter-spacing:0}.hero h1{font-size:44px;line-height:1.05;margin:12px 0 10px;max-width:820px}.hero h2{font-size:24px;line-height:1.2;margin:0 0 18px;color:var(--accent);font-weight:800}.hero p{font-size:18px;line-height:1.65;color:var(--muted);max-width:820px}.hero-cover{width:100%;max-height:420px;object-fit:cover;border-radius:8px;border:1px solid var(--line);box-shadow:0 24px 80px rgba(0,0,0,.5)}.library{padding:20px clamp(16px,4%,56px) 56px}.section{border-top:1px solid var(--line);padding:28px 0}.section-head{display:grid;grid-template-columns:150px minmax(0,1fr);gap:20px;align-items:start}.poster{width:150px;aspect-ratio:2/3;object-fit:cover;border-radius:8px;border:1px solid var(--line);background:var(--panel-2)}.poster-fallback{width:150px;aspect-ratio:2/3;border-radius:8px;border:1px solid var(--line);background:linear-gradient(135deg,#191919,#2a1f1f);display:grid;place-items:center;color:var(--muted);font-weight:800}.section h2{font-size:28px;margin:0 0 10px}.section p{margin:0;color:var(--muted);line-height:1.6}.episodes{display:grid;grid-template-columns:repeat(auto-fit,minmax(260px,1fr));gap:12px;margin-top:22px}.episode{display:block;text-decoration:none;background:var(--panel);border:1px solid var(--line);border-radius:8px;padding:14px 16px;color:var(--text)}.episode:hover{border-color:var(--accent);background:#211817}.episode small{display:block;color:var(--accent-2);margin-top:8px}.empty{padding:32px;border:1px solid var(--line);border-radius:8px;background:var(--panel);color:var(--muted)}@media (max-width:760px){.hero{grid-template-columns:1fr;padding-top:32px}.hero h1{font-size:34px}.section-head{grid-template-columns:96px minmax(0,1fr)}.poster,.poster-fallback{width:96px}})"
       << R"(</style></head><body><main class="shell">)"
       << R"(<header class="topbar"><div class="brand">Roanapur Stream</div><a class="navlink" href="/video/how-it-works">How it works</a></header>)";

  if (!sections.empty()) {
    const SeriesSection &featured = sections.front();
    body << R"(<section class="hero"><div><div class="eyebrow">Now streaming</div><h1>Subaru welcomes you to this page</h1><h2>)"
         << escapeHtml(displayTitle(featured.name)) << R"(</h2><p>)"
         << escapeHtml(getSeriesDescription(featured.name))
         << R"(</p></div>)";

    if (!featured.cover_url.empty()) {
      body << R"(<img class="hero-cover" src=")" << featured.cover_url
           << R"(" alt=")" << escapeHtml(displayTitle(featured.name)) << R"( cover">)";
    }

    body << R"(</section>)";
  }

  body << R"(<section class="library">)";

  if (sections.empty()) {
    body << R"(<div class="empty">No videos found yet. Add MP4, MKV, WebM, MOV, or AVI files anywhere inside the video folder.</div>)";
  }

  for (const SeriesSection &section : sections) {
    body << R"(<article class="section"><div class="section-head">)";

    if (!section.cover_url.empty()) {
      body << R"(<img class="poster" src=")" << section.cover_url << R"(" alt=")"
           << escapeHtml(displayTitle(section.name)) << R"( cover">)";
    } else {
      body << R"(<div class="poster-fallback">)" << escapeHtml(displayTitle(section.name))
           << R"(</div>)";
    }

    body << R"(<div><h2>)" << escapeHtml(displayTitle(section.name)) << R"(</h2><p>)"
         << escapeHtml(getSeriesDescription(section.name))
         << R"(</p></div></div><div class="episodes">)";

    for (const auto &relative_path : section.videos) {
      std::string href = makeVideoUrl(req.path, relative_path.string());
      std::string label = relative_path.filename().string();

      body << R"(<a class="episode" href=")" << href << R"(">)"
           << escapeHtml(displayTitle(label)) << R"(<small>)"
           << escapeHtml(relative_path.parent_path().string())
           << R"(</small></a>)";
    }

    body << R"(</div></article>)";
  }

  body << R"(</section></main></body></html>)";
  return body.str();
}

std::string buildHowItWorksPage() {
  return R"HTML(<!doctype html><html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1"><title>How It Works</title><style>:root{color-scheme:dark;--bg:#0b0b0b;--panel:#171717;--panel-2:#202020;--text:#f3efe6;--muted:#b7aea2;--line:#3a342d;--accent:#e3483b;--accent-2:#65c18c}*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--text);font-family:Inter,Arial,sans-serif}.page{max-width:1100px;margin:0 auto;padding:32px 18px 72px}.nav{display:inline-block;color:var(--muted);text-decoration:none;border:1px solid var(--line);border-radius:8px;padding:10px 14px;margin-bottom:32px}.eyebrow{color:var(--accent-2);font-weight:800;text-transform:uppercase;font-size:13px}h1{font-size:44px;line-height:1.08;margin:10px 0 18px}h2{font-size:24px;margin:0 0 12px}p,li{color:var(--muted);line-height:1.7;font-size:17px}.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(280px,1fr));gap:16px}.panel{border:1px solid var(--line);border-radius:8px;background:var(--panel);padding:22px;margin-top:18px}.wide{margin-top:22px}.flow{display:grid;gap:10px}.flow div{background:var(--panel-2);border:1px solid var(--line);border-radius:8px;padding:12px 14px;color:var(--text)}code{color:var(--accent-2)}strong{color:var(--text)}pre{overflow:auto;background:#090909;border:1px solid var(--line);border-radius:8px;padding:16px;color:var(--muted);line-height:1.5}</style><script type="module">import mermaid from 'https://cdn.jsdelivr.net/npm/mermaid@10/dist/mermaid.esm.min.mjs';mermaid.initialize({ startOnLoad: true, theme: 'dark' });</script></head><body><main class="page"><a class="nav" href="/">Back to library</a><div class="eyebrow">Project details</div><h1>How Subaru's streaming server works</h1><p>This project is a high-performance C++ video streaming backend. It uses raw TCP sockets, custom HTTP parsing, and a modular architecture to serve media files with professional-grade efficiency.</p><section class="panel wide"><h2>System Architecture (HLD)</h2><div class="mermaid">
graph TD
    subgraph "Traffic Flow"
        User([🌐 User Browser]) -->|Public URL| CF[☁️ Cloudflare Tunnel]
        CF -->|Local Port 9000| Server[🚀 C++ Streaming Server]
        Server -->|Recursive Scan| Disk[(📂 Video Storage)]
    end

    subgraph "CI/CD Pipeline"
        GitHub[🐙 GitHub Repo] -- "Push to main" --> Runner[🏃 Self-Hosted Runner]
        Runner -- "Docker Build & Deploy" --> Server
    end
</div></section><section class="panel wide"><h2>Request flow</h2><div class="flow"><div><strong>Routing:</strong> Root <code>/</code> is automatically mapped to the <code>/video/</code> library.</div><div><strong>TCP Server:</strong> Accepts clients on port <code>9000</code> and hands them to a <strong>Thread Pool</strong>.</div><div><strong>HTTP Parser:</strong> Extracts method, path, and headers (like <code>Range</code>).</div><div><strong>Video Handler:</strong> Scans the <code>video</code> folder recursively, finding <strong>.mp4</strong> episodes and <strong>JPEG/AVIF</strong> covers.</div><div><strong>File Streamer:</strong> Executes the dual-mode streaming logic.</div></div></section><section class="grid"><div class="panel"><h2>Dual Streaming Engine</h2><ul><li><strong>🔥 Optimized (Final):</strong> Uses <code>sendfile()</code> for zero-copy transfer. Data moves from Disk → Kernel → Network without passing through User space.</li><li><strong>🔹 Basic (Initial):</strong> A robust <code>read() -> send()</code> fallback that ensures the server works on <strong>Windows</strong> and non-Linux systems.</li></ul></div><div class="panel"><h2>Content Discovery</h2><ul><li><strong>Automatic Indexing:</strong> Scans any folder hierarchy under <code>video/</code>.</li><li><strong>Deep Cover Art:</strong> Automatically finds and displays images (including <code>.jfif</code> and <code>.avif</code>) found deep within your series folders as thumbnails.</li><li><strong>Range Support:</strong> Full support for <code>206 Partial Content</code>, allowing instant seeking and scrubbing.</li></ul></div></section><section class="panel wide"><h2>Cross-Platform Architecture</h2><p>The code is designed to be truly portable. It uses POSIX headers for Linux/Docker production environments while providing native Win32 compatibility for local Windows development, mapping socket and file I/O operations seamlessly across operating systems.</p></section></main></body></html>)HTML";
}

void sendHtmlResponse(int client_fd, const http::HttpRequest &req,
                      const std::string &body) {
  std::string response =
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/html; charset=utf-8\r\n"
      "Content-Length: " +
      std::to_string(body.size()) +
      "\r\n"
      "Connection: close\r\n"
      "\r\n" +
      (req.method == "HEAD" ? "" : body);

  sendAll(client_fd, response);
}

void sendDirectoryListing(int client_fd, const http::HttpRequest &req,
                          const std::string &directory_path) {
  sendHtmlResponse(client_fd, req, buildLibraryPage(req, directory_path));
}

} // namespace

void VideoHandler::handle(int client_fd, const http::HttpRequest &req) {

  if (req.method != "GET" && req.method != "HEAD") {
    const std::string method_not_allowed =
        "HTTP/1.1 405 Method Not Allowed\r\n"
        "Allow: GET, HEAD\r\n"
        "Connection: close\r\n"
        "\r\n";
    sendAll(client_fd, method_not_allowed);
    return;
  }

  std::string decoded_path = decodeUrlPath(req.path);
  if (decoded_path.find("..") != std::string::npos) {
    const std::string bad_request =
        "HTTP/1.1 400 Bad Request\r\n"
        "Connection: close\r\n"
        "\r\n";
    sendAll(client_fd, bad_request);
    return;
  }

  if (decoded_path == "/video/how-it-works" ||
      decoded_path == "/video/how-it-works/") {
    sendHtmlResponse(client_fd, req, buildHowItWorksPage());
    return;
  }

  std::string file_path = resolveVideoPath(decoded_path);

  struct stat st;
  if (stat(file_path.c_str(), &st) < 0) {
    const std::string not_found =
        "HTTP/1.1 404 Not Found\r\n"
        "Connection: close\r\n"
        "\r\n";
    sendAll(client_fd, not_found);
    return;
  }

  if (isDirectory(file_path)) {
    sendDirectoryListing(client_fd, req, file_path);
    return;
  }

  size_t file_size = st.st_size;

  std::string range_header;
  if (req.headers.count("Range")) {
    range_header = req.headers.at("Range");
  }

  Range r = RangeParser::parse(range_header, file_size);
  if (!range_header.empty() && !r.valid) {
    const std::string range_not_satisfiable =
        "HTTP/1.1 416 Range Not Satisfiable\r\n"
        "Content-Range: bytes */" +
        std::to_string(file_size) +
        "\r\n"
        "Connection: close\r\n"
        "\r\n";
    sendAll(client_fd, range_not_satisfiable);
    return;
  }

  size_t start = r.valid ? r.start : 0;
  size_t end = r.valid ? r.end : file_size - 1;

  size_t content_length = end - start + 1;

  std::string response =
      std::string("HTTP/1.1 ") + (r.valid ? "206 Partial Content" : "200 OK") +
      "\r\n"
      "Content-Type: " +
      getContentType(file_path) +
      "\r\n"
      "Content-Length: " +
      std::to_string(content_length) +
      "\r\n"
      "Accept-Ranges: bytes\r\n"
      "Connection: close\r\n";

  if (r.valid) {
    response += "Content-Range: bytes " + std::to_string(start) + "-" +
                std::to_string(end) + "/" + std::to_string(file_size) + "\r\n";
  }

  response += "\r\n";

  if (!sendAll(client_fd, response) || req.method == "HEAD")
    return;

  FileStreamer::stream(client_fd, file_path, start, end);
}

} // namespace streaming
