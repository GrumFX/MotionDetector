#include "WebServer.h"
#include "crow/crow.h"
#include "SQLiteDB.h"
#include "Measurement.h"
#include <sstream>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <vector>
#include <algorithm>

extern SQLiteDB db;

static std::string nowMinus24h()
{
    std::time_t now = std::time(nullptr);
    std::time_t before = now - 24 * 3600;
    std::tm* t = std::localtime(&before);
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", t);
    return buf;
}

static std::string now()
{
    std::time_t now = std::time(nullptr);
    std::tm* t = std::localtime(&now);
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", t);
    return buf;
}

// Helper function to escape special characters in strings
std::string escapeString(const std::string& str)
{
    std::ostringstream escaped;
    for (unsigned char c : str)
    {
        if (c < 32 || c >= 127)
        {
            escaped << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c);
        }
        else
        {
            escaped << c;
        }
    }
    return escaped.str();
}

std::string sanitizeSSID(const std::string& ssid)
{
    return escapeString(ssid);
}

std::string buildHTMLPage(const std::string& from, const std::string& to, const std::vector<Measurement>& measurements, const std::vector<Motion>& motions, const std::string& displayMode)
{
    std::ostringstream html;

    html << R"HTML(
    <!DOCTYPE html>
    <html>
    <head>
        <meta charset="UTF-8">
        <title>Motion Detector</title>
        <script src="https://cdn.plot.ly/plotly-latest.min.js"></script>       
        <style>
            body {
                font-family: Arial, sans-serif;
                padding: 12px;
                margin: 0;
            }
            h1 {
                margin: 0 0 10px 0;
                font-size: 20px;
            }
            form {
                margin-bottom: 10px;
                font-size: 14px;
            }
            table {
                border-collapse: collapse;
                width: 100%;
                margin-bottom: 20px;
            }
            th, td {
                border: 1px solid #ccc;
                padding: 6px 10px;
                text-align: left;
                font-size: 13px;
            }
            th {
                background-color: #f2f2f2;
            }
            h2 {
                font-size: 16px;
                margin: 20px 0 10px;
            }
            label {
                margin-right: 12px;
            }
            #chartContainer {
                height: 260px;
                max-height: 35vh;
                margin-bottom: 0;
            }
        </style>
    </head>
    <body>
        <h1>Motion Detector Interface</h1>
        <form id="filterForm" method="get" action="/">
            <label>From: <input type="datetime-local" name="from" value=")HTML"
        << from.substr(0, 10) << "T" << from.substr(11, 5)
        << R"HTML("></label>
            <label>To: <input type="datetime-local" name="to" value=")HTML"
        << to.substr(0, 10) << "T" << to.substr(11, 5)
        << R"HTML("></label>
            <input type="submit" value="Show manually">
            <button type="button" onclick="quickRange(1)">Last Hour</button>
            <button type="button" onclick="quickRange(24)">Last Day</button>
            <br><br>
            <label>Display:
                <select name="mode" onchange="updateChart();">
                    <option value="all" )HTML"
        << (displayMode == "all" ? "selected" : "") << R"HTML(>All Measurements</option>
                    <option value="motion" )HTML"
        << (displayMode == "motion" ? "selected" : "") << R"HTML(>Motion Only</option>
                </select>
            </label>
            <label>Filter SSID:
                <select id="ssidFilter" onchange="updateChart()">
                    <option value="">All</option>
                </select>
            </label>
            <label>Filter Source:
                <select id="sourceFilter" onchange="updateChart()">
                    <option value="">All</option>
                </select>
            </label>
        </form>

        <h2>RSSI Signal Chart</h2>
        <div id="chartContainer">
            <div id="rssiChart"></div>
        </div>
    )HTML";

    html << R"HTML(
        <script>
        const rawData = [
    )HTML";

    // Combine Measurements and Motions
    std::vector<std::string> combinedData;
    for (const auto& m : measurements)
    {
        if (m.timeStamp.size() != 19 || m.timeStamp[10] != ' ' || m.timeStamp[13] != ':' || m.timeStamp[16] != ':')
        {
            continue; // Skip invalid entries
        }

        std::string sanitizedSSID = sanitizeSSID(m.ssid);
        combinedData.push_back("{ time: '" + m.timeStamp + "', ssid: '" + sanitizedSSID
            + "', source: '" + m.source + "', rssi: " + std::to_string(m.rssi) + " },");
    }

    for (const auto& m : motions)
    {
        if (m.timeStamp.size() != 19 || m.timeStamp[10] != ' ' || m.timeStamp[13] != ':' || m.timeStamp[16] != ':')
        {
            continue; // Skip invalid entries
        }

        std::string sanitizedSSID = sanitizeSSID(m.ssid);
        combinedData.push_back("{ time: '" + m.timeStamp + "', ssid: '" + sanitizedSSID
            + "', source: '" + m.source + "', rssi: " + std::to_string(m.rssi) + " },");
    }

    for (const auto& entry : combinedData)
    {
        html << entry << "\n";
    }

    html << R"HTML(
        ];
        const grouped = {};
        const ssidSet = new Set();
        const sourceSet = new Set();

        rawData.forEach(entry => {
            ssidSet.add(entry.ssid);
            sourceSet.add(entry.source);
            if (!grouped[entry.ssid]) grouped[entry.ssid] = [];
            grouped[entry.ssid].push({ x: entry.time, y: entry.rssi, source: entry.source });
        });

        const traces = Object.keys(grouped).map(ssid => ({
            x: grouped[ssid].map(point => point.x),
            y: grouped[ssid].map(point => point.y),
            mode: 'lines',
            name: ssid,
            type: 'scatter'
        }));

        try {
            Plotly.newPlot('rssiChart', traces, {
                title: 'Wi-Fi Signal Strength Over Time',
                xaxis: {
                    title: 'Time',
                    type: 'date'
                },
                yaxis: {
                    title: 'Signal Strength (dBm)',
                    autorange: 'reversed'
                },
                legend: {
                    orientation: 'h',
                    xanchor: 'center',
                    x: 0.5,
                    y: -0.2
                }
            });

            function updateChart() {
                const ssidFilter = document.getElementById('ssidFilter').value;
                const sourceFilter = document.getElementById('sourceFilter').value;
                const displayMode = document.querySelector('select[name="mode"]').value;

                // Clear existing traces
                Plotly.purge('rssiChart');

                // Filter data based on selected options
                const filteredData = rawData.filter(entry => {
                    const matchesSSID = ssidFilter === '' || entry.ssid === ssidFilter;
                    const matchesSource = sourceFilter === '' || entry.source === sourceFilter;
                    const showMotionOnly = displayMode === 'motion' && entry.source.includes('motion');
                    return matchesSSID && matchesSource && (displayMode !== 'motion' || showMotionOnly);
                });

                // Group filtered data
                const filteredGrouped = {};
                filteredData.forEach(entry => {
                    if (!filteredGrouped[entry.ssid]) filteredGrouped[entry.ssid] = [];
                    filteredGrouped[entry.ssid].push({ x: entry.time, y: entry.rssi, source: entry.source });
                });

                const filteredTraces = Object.keys(filteredGrouped).map(ssid => ({
                    x: filteredGrouped[ssid].map(point => point.x),
                    y: filteredGrouped[ssid].map(point => point.y),
                    mode: 'lines',
                    name: ssid,
                    type: 'scatter'
                }));

                // Redraw the chart with filtered data
                Plotly.newPlot('rssiChart', filteredTraces, {
                    title: 'Wi-Fi Signal Strength Over Time',
                    xaxis: {
                        title: 'Time',
                        type: 'date'
                    },
                    yaxis: {
                        title: 'Signal Strength (dBm)',
                        autorange: 'reversed'
                    },
                    legend: {
                        orientation: 'h',
                        xanchor: 'center',
                        x: 0.5,
                        y: -0.2
                    }
                });
            }

            ssidSet.forEach(ssid => {
                const opt = document.createElement("option");
                opt.value = opt.text = ssid;
                document.getElementById("ssidFilter").appendChild(opt);
            });
            sourceSet.forEach(src => {
                const opt = document.createElement("option");
                opt.value = opt.text = src;
                document.getElementById("sourceFilter").appendChild(opt);
            });

            function quickRange(hours) {
                const now = new Date();
                const past = new Date(now.getTime() - hours * 3600 * 1000);
                const fmt = d => d.toISOString().slice(0, 16);
                document.querySelector("input[name='from']").value = fmt(past);
                document.querySelector("input[name='to']").value = fmt(now);
                document.getElementById("filterForm").submit();
            }

            setInterval(() => document.getElementById("filterForm").submit(), 30000);
        } catch (error) {
            console.error("Error initializing chart:", error);
        }
        </script>
    </body>
    </html>
    )HTML";

    return html.str();
}

void start_webserver()
{
    crow::SimpleApp app;

    CROW_ROUTE(app, "/")
        ([](const crow::request& req)
    {
        std::string from = req.url_params.get("from") ? req.url_params.get("from") : nowMinus24h();
        std::string to = req.url_params.get("to") ? req.url_params.get("to") : now();
        std::string mode = req.url_params.get("mode") ? req.url_params.get("mode") : "all";

        std::replace(from.begin(), from.end(), 'T', ' ');
        std::replace(to.begin(), to.end(), 'T', ' ');
        if (from.size() == 16) from += ":00";
        if (to.size() == 16) to += ":59";

        std::vector<Measurement> measurements;
        std::vector<Motion> motions;

        if (mode == "motion")
        {
            // Don't show regular measurements
            db.readMotion(from, to, motions);
        }
        else
        {
            // Default: show both signals and motion events
            db.readSignal(from, to, measurements);
            db.readMotion(from, to, motions);
        }

        std::cout << "motions size: " << motions.size() << std::endl;
        for (const auto& m : motions)
        {
            std::cout << "Time: " << m.timeStamp << ", Sourse: " << m.source << ", SSID: " << m.ssid << ", RSSI: " << m.rssi << std::endl;
        }

        return crow::response{ buildHTMLPage(from, to, measurements, motions) };
    });

    std::cout << "Web server running at http://<IP>:9002/" << std::endl;
    app.bindaddr("0.0.0.0").port(9002).multithreaded().run();
}