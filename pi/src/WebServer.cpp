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

std::string buildHTMLPage(const std::string& from, const std::string& to, const std::vector<Measurement>& measurements, const std::vector<Motion>& motions)
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
                <select name="mode" onchange="document.getElementById('filterForm').submit();">
                    <option value="all">All Measurements</option>
                    <option value="motion">Motion Only</option>
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

    for (const auto& m : measurements)
    {
        // Ensure timeStamp is in the correct format
        if (m.timeStamp.size() != 19 || m.timeStamp[10] != ' ' || m.timeStamp[13] != ':' || m.timeStamp[16] != ':')
        {
            continue; // Skip invalid entries
        }

        // Sanitize and escape SSID
        std::string sanitizedSSID = sanitizeSSID(m.ssid);

        html << "{ time: '" << m.timeStamp << "', ssid: '" << sanitizedSSID
            << "', source: '" << m.source << "', rssi: " << m.rssi << " },\n";
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
                const ssid = document.getElementById('ssidFilter').value;
                const source = document.getElementById('sourceFilter').value;
                traces.forEach(trace => {
                    trace.visible = !ssid || trace.name === ssid;
                });
                Plotly.redraw('rssiChart');
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

        std::replace(from.begin(), from.end(), 'T', ' ');
        std::replace(to.begin(), to.end(), 'T', ' ');
        if (from.size() == 16) from += ":00";
        if (to.size() == 16) to += ":59";

        std::vector<Measurement> measurements;
        std::vector<Motion> motions;

        // Debug: Print measurements before building the HTML
        db.readSignal(from, to, measurements);
        std::cout << "Measurements size: " << measurements.size() << std::endl;
        for (const auto& m : measurements)
        {
            std::cout << "Time: " << m.timeStamp << ", SSID: " << m.ssid << ", RSSI: " << m.rssi << std::endl;
        }

        db.readMotion(from, to, motions);

        return crow::response{ buildHTMLPage(from, to, measurements, motions) };
    });

    std::cout << "Web server running at http://<IP>:9002/" << std::endl;
    app.bindaddr("0.0.0.0").port(9002).multithreaded().run();
}