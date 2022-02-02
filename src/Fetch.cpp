#include "Fetch.h"
#include <WiFiClientSecure.h>

Response fetch(const char* url, RequestOptions options) {
    // Parsing URL.
    Url parsedUrl = parseUrl(url);
    
    WiFiClientSecure client;
    // Retry every 15 seconds.
    client.setTimeout(15000);

    // Set fingerprint if https.
    if(parsedUrl.scheme == "https") {
        #ifdef ESP8266
        if(options.fingerprint == "" && options.caCert == "") {
            Serial.println("[INFO] No fingerprint or caCert is provided. Using the INSECURE mode for connection!");
            client.setInsecure();
        }
        else if(options.caCert != "") client.setTrustAnchors(new X509List(options.caCert.c_str()));
        else client.setFingerprint(options.fingerprint.c_str());
        #elif defined(ESP32)
        if(options.caCert == "") {
            Serial.println("[INFO] No CA Cert is provided. Using the INSECURE mode for connection!");
            client.setInsecure();
        }
        else client.setCACert(options.caCert.c_str());
        #endif
    }

    // Connecting to server.
    while(!client.connect(parsedUrl.host.c_str(), parsedUrl.port)) {
        delay(1000);
        Serial.print(".");
    }

    // Forming request.
    String request =
        options.method + " " + parsedUrl.path + parsedUrl.afterPath + " HTTP/1.1\r\n" +
        "Host: " + parsedUrl.host + "\r\n" +
        "User-Agent: " + options.headers.userAgent + "\r\n" +
        "Content-Type: " + options.headers.contentType + "\r\n" +
        "Connection: " + options.headers.connection + "\r\n\r\n" +
        options.body + "\r\n\r\n";

    Serial.println("Request is: ");
    Serial.println(request);

    // Sending request.
    client.print(request);

    // Getting response headers.
    Response response;
    for(int nLine = 1; client.connected(); nLine++) {
        // Reading headers line by line.
        String line = client.readStringUntil('\n');
        // Parse status and statusText from line 1.
        if(nLine == 1) {
            response.status = line.substring(line.indexOf(" ")).substring(0, line.indexOf(" ")).toInt();
            response.statusText = line.substring(line.indexOf(String(response.status)) + 4);
            response.statusText.trim();
            continue;
        }

        response.headers.text += line + "\n";
        // If headers end, move on.
        if(line == "\r") break;
    }

    Serial.println("-----HEADERS START-----");
    Serial.println(response.headers.text);
    Serial.println("-----HEADERS END-----");

    // Getting response body.
    while(client.available()) {
        response.body += client.readStringUntil('\n');
    }

    return response;
}

Headers::Headers(): contentType("application/x-www-form-urlencoded"),
    contentLength(-1), userAgent("arduino-fetch"), cookie(""),
    accept("*/*"), connection("close"), transferEncoding("") {}

ResponseHeaders::ResponseHeaders(): text("") {}

String ResponseHeaders::get(String headerName) {
    String tillEnd = this->text.substring(this->text.lastIndexOf(headerName + ": "));
    return tillEnd.substring(tillEnd.indexOf(" ") + 1, tillEnd.indexOf("\n") - 1);
}

Body::Body(): _text("") {}

String Body::text() {
    return this->_text;
}

String Body::operator+(String str) {
    return this->_text + str;
}

String Body::operator=(String str) {
    return this->_text = str;
}

String operator+(String str, Body body) {
    return str + body.text();
}

Response::Response(): ok(false), status(200), statusText("OK"),
    redirected(false), type(""), headers(ResponseHeaders()), body("") {}

String Response::text() {
    return this->body;
}

size_t Response::printTo(Print& p) const {
    size_t r = 0;

    r += p.printf(
        (String("{") +
            "\n\t\"ok\": %d" +
            "\n\t\"status\": %d" +
            "\n\t\"statusText\": \"%s\"" +
            "\n\t\"body\": \"%s\"" +
        "\n}").c_str(),
        ok, status, statusText.c_str(), body.c_str());

    return r;
}

RequestOptions::RequestOptions(): method("GET"), headers(Headers()), body(Body()),
#if defined(ESP8266)
fingerprint(""),
#endif
caCert("") {}

