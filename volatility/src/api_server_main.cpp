

#include <iostream>
#include <string>
#include <csignal>
#include <atomic>
#include <thread>
#include <chrono>
#include <sstream>
#include <cstring>

#include "vol_api.hpp"
#include <nlohmann/json.hpp>

#if __has_include("httplib.h")
    #define HTTPLIB_AVAILABLE 1
    #include "httplib.h"
#else
    #define HTTPLIB_AVAILABLE 0
#endif

static std::atomic<bool> g_shutdown{false};

void signalHandler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down...\n";
    g_shutdown = true;
}

void printUsage(const char* programName) {
    std::cout << "\n";
    std::cout << "  Volatility Arbitrage REST API Server\n";
    std::cout << "\n\n";
    std::cout << "Usage: " << programName << " [OPTIONS] [PORT]\n\n";
    std::cout << "Options:\n";
    std::cout << "  --help, -h     Show this help message\n";
    std::cout << "  --port, -p     Port to listen on (default: 8080)\n";
    std::cout << "  --host         Host to bind to (default: 0.0.0.0)\n";
    std::cout << "  --console      Run in console mode (no HTTP server)\n\n";
    std::cout << "Examples:\n";
    std::cout << "  " << programName << " 8080\n";
    std::cout << "  " << programName << " --port 9000\n";
    std::cout << "  " << programName << " --host 127.0.0.1 --port 8080\n\n";
    std::cout << "Endpoints:\n";
    std::cout << "  POST /api/v1/arbitrage/check   - Detect arbitrage violations\n";
    std::cout << "  POST /api/v1/arbitrage/repair  - Repair surface using QP\n";
    std::cout << "  POST /api/v1/arbitrage/quality - Analyze surface quality\n";
    std::cout << "  POST /api/v1/svi/fit           - Fit SVI parameters\n";
    std::cout << "  POST /api/v1/localvol/compute  - Compute Dupire local vol\n";
    std::cout << "  GET  /api/v1/status            - System status\n";
    std::cout << "  GET  /api/v1/config            - Current configuration\n";
    std::cout << "  POST /api/v1/config            - Update configuration\n";
    std::cout << "  GET  /api/v1/metrics           - Performance metrics\n";
    std::cout << "  GET  /health                   - Health check\n";
}

void runConsoleMode() {
    std::cout << "\n\n";
    std::cout << "  Volatility Arbitrage API - Console Mode\n";
    std::cout << "\n\n";
    std::cout << "Available commands:\n";
    std::cout << "  status     - Show system status\n";
    std::cout << "  config     - Show current configuration\n";
    std::cout << "  metrics    - Show performance metrics\n";
    std::cout << "  health     - Run health check\n";
    std::cout << "  version    - Show API version\n";
    std::cout << "  help       - Show this help\n";
    std::cout << "  quit/exit  - Exit the application\n\n";
    std::cout << "For arbitrage detection, use the command-line tool: vol_arb.exe <data_file>\n\n";
    
    RestAPIHandler handler;
    auto& api = VolatilityArbitrageAPI::getInstance();
    
    std::string line;
    while (!g_shutdown) {
        std::cout << "vol_api> ";
        std::cout.flush();
        
        if (!std::getline(std::cin, line)) {
            break;
        }

        size_t start = line.find_first_not_of(" \t\r\n");
        size_t end = line.find_last_not_of(" \t\r\n");
        if (start == std::string::npos) continue;
        line = line.substr(start, end - start + 1);
        
        if (line.empty()) continue;
        
        if (line == "quit" || line == "exit" || line == "q") {
            std::cout << "Goodbye!\n";
            break;
        } else if (line == "status") {
            auto response = api.getStatus();
            std::cout << handler.serializeResponse(response) << "\n\n";
        } else if (line == "config") {
            auto response = api.getConfiguration();
            std::cout << handler.serializeResponse(response) << "\n\n";
        } else if (line == "metrics") {
            auto response = api.getPerformanceMetrics();
            std::cout << handler.serializeResponse(response) << "\n\n";
        } else if (line == "health") {
            bool healthy = api.healthCheck();
            std::cout << "Health check: " << (healthy ? "PASSED" : "FAILED") << "\n\n";
        } else if (line == "version") {
            std::cout << "API Version: " << api.getVersion() << "\n\n";
        } else if (line == "help" || line == "?") {
            std::cout << "\nAvailable commands:\n";
            std::cout << "  status     - Show system status\n";
            std::cout << "  config     - Show current configuration\n";
            std::cout << "  metrics    - Show performance metrics\n";
            std::cout << "  health     - Run health check\n";
            std::cout << "  version    - Show API version\n";
            std::cout << "  help       - Show this help\n";
            std::cout << "  quit/exit  - Exit the application\n\n";
        } else {
            std::cout << "Unknown command: " << line << "\n";
            std::cout << "Type 'help' for available commands.\n\n";
        }
    }
}

#if HTTPLIB_AVAILABLE

void runHttpServer(const std::string& host, int port) {
    httplib::Server server;
    RestAPIHandler handler;

    auto addCorsHeaders = [](httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
    };

    server.Options("/(.*)", [&addCorsHeaders](const httplib::Request&, httplib::Response& res) {
        addCorsHeaders(res);
        res.status = 204;
    });

    server.Get("/health", [&](const httplib::Request&, httplib::Response& res) {
        auto& api = VolatilityArbitrageAPI::getInstance();
        bool healthy = api.healthCheck();
        
        nlohmann::json response;
        response["status"] = healthy ? "healthy" : "unhealthy";
        response["version"] = api.getVersion();
        response["timestamp"] = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
        
        addCorsHeaders(res);
        res.set_content(response.dump(2), "application/json");
        res.status = healthy ? 200 : 503;
    });

    server.Get("/api/v1/status", [&](const httplib::Request&, httplib::Response& res) {
        auto& api = VolatilityArbitrageAPI::getInstance();
        auto response = api.getStatus();
        
        addCorsHeaders(res);
        res.set_content(handler.serializeResponse(response), "application/json");
        res.status = response.success ? 200 : 500;
    });

    server.Get("/api/v1/config", [&](const httplib::Request&, httplib::Response& res) {
        auto& api = VolatilityArbitrageAPI::getInstance();
        auto response = api.getConfiguration();
        
        addCorsHeaders(res);
        res.set_content(handler.serializeResponse(response), "application/json");
        res.status = 200;
    });
    
    server.Post("/api/v1/config", [&](const httplib::Request& req, httplib::Response& res) {
        auto& api = VolatilityArbitrageAPI::getInstance();
        auto response = api.updateConfiguration(req.body);
        
        addCorsHeaders(res);
        res.set_content(handler.serializeResponse(response), "application/json");
        res.status = response.success ? 200 : 400;
    });

    server.Get("/api/v1/metrics", [&](const httplib::Request&, httplib::Response& res) {
        auto& api = VolatilityArbitrageAPI::getInstance();
        auto response = api.getPerformanceMetrics();
        
        addCorsHeaders(res);
        res.set_content(handler.serializeResponse(response), "application/json");
        res.status = 200;
    });

    server.Post("/api/v1/arbitrage/check", [&](const httplib::Request& req, httplib::Response& res) {
        std::string result = handler.handlePostArbitrageCheck(req.body);
        
        addCorsHeaders(res);
        res.set_content(result, "application/json");
        
        try {
            auto j = nlohmann::json::parse(result);
            res.status = j["success"].get<bool>() ? 200 : 400;
        } catch (...) {
            res.status = 500;
        }
    });

    server.Post("/api/v1/arbitrage/repair", [&](const httplib::Request& req, httplib::Response& res) {
        std::string result = VolApi::repair_surface(req.body);
        
        addCorsHeaders(res);
        res.set_content(result, "application/json");
        
        try {
            auto j = nlohmann::json::parse(result);
            res.status = j["success"].get<bool>() ? 200 : 400;
        } catch (...) {
            res.status = 500;
        }
    });

    server.Post("/api/v1/arbitrage/quality", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            ArbitrageCheckRequest request = handler.parseArbitrageRequest(req.body);
            auto& api = VolatilityArbitrageAPI::getInstance();
            auto response = api.analyzeQuality(request);
            
            addCorsHeaders(res);
            res.set_content(handler.serializeResponse(response), "application/json");
            res.status = response.success ? 200 : 400;
        } catch (const std::exception& e) {
            addCorsHeaders(res);
            res.set_content(handler.createErrorResponse(e.what(), 400), "application/json");
            res.status = 400;
        }
    });

    server.Post("/api/v1/svi/fit", [&](const httplib::Request& req, httplib::Response& res) {
        std::string result = VolApi::fit_svi(req.body);
        
        addCorsHeaders(res);
        res.set_content(result, "application/json");
        
        try {
            auto j = nlohmann::json::parse(result);
            res.status = j["success"].get<bool>() ? 200 : 400;
        } catch (...) {
            res.status = 500;
        }
    });

    server.Post("/api/v1/localvol/compute", [&](const httplib::Request& req, httplib::Response& res) {
        std::string result = VolApi::compute_local_vol(req.body);
        
        addCorsHeaders(res);
        res.set_content(result, "application/json");
        
        try {
            auto j = nlohmann::json::parse(result);
            res.status = j["success"].get<bool>() ? 200 : 400;
        } catch (...) {
            res.status = 500;
        }
    });

    server.Post("/api/v1/arbitrage/batch", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            nlohmann::json j = nlohmann::json::parse(req.body);
            
            if (!j.contains("requests") || !j["requests"].is_array()) {
                throw std::invalid_argument("Missing 'requests' array");
            }
            
            std::vector<ArbitrageCheckRequest> requests;
            for (const auto& reqJson : j["requests"]) {
                requests.push_back(handler.parseArbitrageRequest(reqJson.dump()));
            }
            
            auto& api = VolatilityArbitrageAPI::getInstance();
            auto response = api.batchCheckArbitrage(requests);
            
            addCorsHeaders(res);
            res.set_content(handler.serializeResponse(response), "application/json");
            res.status = response.success ? 200 : 400;
        } catch (const std::exception& e) {
            addCorsHeaders(res);
            res.set_content(handler.createErrorResponse(e.what(), 400), "application/json");
            res.status = 400;
        }
    });

    server.set_error_handler([&](const httplib::Request&, httplib::Response& res) {
        nlohmann::json error;
        error["success"] = false;
        error["error"]["code"] = res.status;
        error["error"]["message"] = "Request failed";
        
        res.set_content(error.dump(), "application/json");
    });
    
    std::cout << "\n\n";
    std::cout << "  Volatility Arbitrage REST API Server\n";
    std::cout << "\n\n";
    std::cout << "Server starting on http://" << host << ":" << port << "\n\n";
    std::cout << "Available endpoints:\n";
    std::cout << "  GET  /health                   - Health check\n";
    std::cout << "  GET  /api/v1/status            - System status\n";
    std::cout << "  GET  /api/v1/config            - Current configuration\n";
    std::cout << "  POST /api/v1/config            - Update configuration\n";
    std::cout << "  GET  /api/v1/metrics           - Performance metrics\n";
    std::cout << "  POST /api/v1/arbitrage/check   - Detect arbitrage violations\n";
    std::cout << "  POST /api/v1/arbitrage/repair  - Repair surface using QP\n";
    std::cout << "  POST /api/v1/arbitrage/quality - Analyze surface quality\n";
    std::cout << "  POST /api/v1/arbitrage/batch   - Batch processing\n";
    std::cout << "  POST /api/v1/svi/fit           - Fit SVI parameters\n";
    std::cout << "  POST /api/v1/localvol/compute  - Compute Dupire local vol\n\n";
    std::cout << "Press Ctrl+C to stop the server.\n";
    std::cout << "\n\n";
    
    if (!server.listen(host.c_str(), port)) {
        std::cerr << "Error: Failed to start server on " << host << ":" << port << "\n";
        std::cerr << "Make sure the port is not already in use.\n";
        return;
    }
}

#else

void runHttpServer(const std::string& host, int port) {
    std::cout << "\n\n";
    std::cout << "  Volatility Arbitrage REST API Server\n";
    std::cout << "\n\n";
    std::cout << "WARNING: cpp-httplib is not available.\n";
    std::cout << "HTTP server functionality is disabled.\n\n";
    std::cout << "To enable the HTTP server, download httplib.h from:\n";
    std::cout << "  https://github.com/yhirose/cpp-httplib/blob/master/httplib.h\n\n";
    std::cout << "Place it in the include/ directory and rebuild.\n\n";
    std::cout << "Running in console mode instead...\n";
    
    runConsoleMode();
}

#endif

int main(int argc, char* argv[]) {
    
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    std::string host = "0.0.0.0";
    int port = 8080;
    bool consoleMode = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            return 0;
        } else if (arg == "--port" || arg == "-p") {
            if (i + 1 < argc) {
                try {
                    port = std::stoi(argv[++i]);
                } catch (...) {
                    std::cerr << "Error: Invalid port number\n";
                    return 1;
                }
            }
        } else if (arg == "--host") {
            if (i + 1 < argc) {
                host = argv[++i];
            }
        } else if (arg == "--console" || arg == "-c") {
            consoleMode = true;
        } else if (arg[0] != '-') {
            
            try {
                port = std::stoi(arg);
            } catch (...) {
                std::cerr << "Error: Invalid port number: " << arg << "\n";
                return 1;
            }
        } else {
            std::cerr << "Error: Unknown option: " << arg << "\n";
            printUsage(argv[0]);
            return 1;
        }
    }

    if (port < 1 || port > 65535) {
        std::cerr << "Error: Port must be between 1 and 65535\n";
        return 1;
    }

    auto& api = VolatilityArbitrageAPI::getInstance();
    if (!api.healthCheck()) {
        std::cerr << "Warning: Initial health check failed\n";
    }

    if (consoleMode) {
        runConsoleMode();
    } else {
        runHttpServer(host, port);
    }
    
    std::cout << "Server stopped.\n";
    return 0;
}
