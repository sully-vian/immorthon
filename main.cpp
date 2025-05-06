#include <algorithm>
#include <atomic>
#include <ctime>
#include <curl/curl.h>
#include <fstream>
#include <iostream>
#include <libxml/HTMLparser.h>
#include <libxml/xpath.h>
#include <omp.h>
#include <vector>

bool verbose = false;
const std::string columns = "word,definition\n";
const std::string searchUrl =
    "https://www.oxfordlearnersdictionaries.com/search/english/?q=";

// find span with class "def" inside div with id "entryContent"
const xmlChar *xPathExpr =
    (const xmlChar *)"//div[@id='entryContent']//span[@class='def']";

void printError(const std::string &msg) {
    if (verbose) {
#pragma omp critical(cerr)
        {
            // erase full line and move to the beginning
            std::cerr << "\033[2K\r";
            // print error message
            std::cerr << msg << std::endl;
        }
    }
}

void printProgressBar(const int progress, const int total) {
    const int barWidth = 50;
    float percentage = static_cast<float>(progress) / total;
    int pos = static_cast<int>(barWidth * percentage);

#pragma omp critical(cout)
    {
        std::cout << "\r[";
        for (int i = 0; i < barWidth; ++i) {
            if (i < pos) {
                std::cout << "#";
            } else {
                std::cout << "-";
            }
        }
        std::cout << "] " << progress << "/" << total << " ("
                  << int(percentage * 100.0) << "%)";
        std::cout.flush();
    }
}

// Callback function to write the response data into a string
size_t writeCallback(void *contents, size_t size, size_t nmemb,
                     void *userdata) {
    std::string *html = static_cast<std::string *>(userdata);
    html->append(static_cast<char *>(contents), size * nmemb);
    return size * nmemb;
}

// Function to extract the definition using libxml2
std::string extractDefinition(const std::string &html) {
    // Parse HTML
    htmlDocPtr doc = htmlReadMemory(html.c_str(), html.size(), NULL, NULL,
                                    HTML_PARSE_RECOVER | HTML_PARSE_NOERROR |
                                        HTML_PARSE_NOWARNING);
    if (!doc) {
        // printError("Failed to parse HTML.");
        return "";
    }

    // Create XPath context
    xmlXPathContextPtr xpathCtx = xmlXPathNewContext(doc);
    if (!xpathCtx) {
        // printError("Failed to create XPath context.");
        xmlFreeDoc(doc);
        return "";
    }

    xmlXPathObjectPtr xpathObj;

    // get the first span with class "def" inside the div with id "entryContent"
    xpathObj = xmlXPathEvalExpression(xPathExpr, xpathCtx);
    if (!xpathObj || !xpathObj->nodesetval ||
        xpathObj->nodesetval->nodeNr == 0) {
        // printError("Failed to evaluate XPath expression");
        xmlXPathFreeObject(xpathObj);
        xmlXPathFreeContext(xpathCtx);
        xmlFreeDoc(doc);
        return "";
    }

    // Extract the first matching node
    xmlNodePtr node = xpathObj->nodesetval->nodeTab[0];
    std::string definition;

    // extract recursively all text nodes
    for (xmlNodePtr child = node->children; child; child = child->next) {
        if (child->type == XML_TEXT_NODE) {
            // if text, append to definition
            definition += (const char *)child->content;
        } else if (child->type == XML_ELEMENT_NODE) {
            // check if "a" tag with "Ref" class
            xmlChar *classAttr = xmlGetProp(child, (const xmlChar *)"class");
            if (classAttr && xmlStrEqual(child->name, (const xmlChar *)"a") &&
                xmlStrEqual(classAttr, (const xmlChar *)"Ref")) {
                xmlChar *childContent = xmlNodeGetContent(child);
                if (childContent) {
                    definition += (const char *)childContent;
                    xmlFree(childContent);
                }
            }
            if (classAttr) {
                xmlFree(classAttr);
            }
        }
    }

    // Cleanup
    xmlXPathFreeObject(xpathObj);
    xmlXPathFreeContext(xpathCtx);
    xmlFreeDoc(doc);

    // Trim
    definition.erase(definition.find_last_not_of(" \n\r\t") + 1);
    return definition;
}

// Function to fetch HTML for a given word
std::string fetchHtml(const std::string &word) {
    const std::string fullUrl =
        searchUrl + curl_easy_escape(nullptr, word.c_str(), word.size());

    CURL *curl = curl_easy_init();
    if (!curl) {
        // printError("Failed to initialize curl.");
        return "";
    }

    std::string html;
    curl_easy_setopt(curl, CURLOPT_URL, fullUrl.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &html);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        // printError("curl_easy_perform() failed for word '" + word +
        //            "': " + curl_easy_strerror(res));
        curl_easy_cleanup(curl);
        return "";
    }

    curl_easy_cleanup(curl);
    return html;
}

// Function to read words from a corpus file
std::vector<std::string> readCorpus(const std::string &filename) {
    std::vector<std::string> words;
    std::ifstream file(filename);
    if (!file.is_open()) {
        printError("Failed to open corpus file: " + filename);
        return words;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty()) {
            words.push_back(line);
        }
    }

    file.close();
    return words;
}

std::string escapeForCsv(const std::string &str) {
    std::string escaped = str;
    // Escape double quotes
    size_t pos = 0;
    while ((pos = escaped.find('"', pos)) != std::string::npos) {
        escaped.insert(pos, "\"");
        pos += 2; // Move past the inserted quote
    }
    return escaped;
}

int main(int argc, char *argv[]) {
    std::time_t startTime = std::time(nullptr);

    if (argc < 3 || argc > 4) {
        printError("Usage: " + std::string(argv[0]) +
                   " [--verbose] <corpus_file> <output_file.csv>");
        return 1;
    }

    if (argc == 4 && std::string(argv[1]) == "--verbose") {
        verbose = true;
    }
    std::string corpusFile = argv[argc - 2];
    std::string dicoFile = argv[argc - 1];

#ifdef _OPENMP
    std::cout << "OpenMP is enabled." << std::endl;
#else
    std::cout << "OpenMP is not enabled." << std::endl;
#endif

    // Read words from the corpus file
    std::vector<std::string> words = readCorpus(corpusFile);
    if (words.empty()) {
        printError("No words found in the corpus file.");
        return 1;
    }

    std::cout << "Fetching definitions for " << words.size() << " words..."
              << std::endl;

    // open dico.csv
    FILE *file = fopen(dicoFile.c_str(), "w");
    if (!file) {
        printError("Failed to open " + dicoFile + " for writing.");
        return 1;
    }

    // write header to dico.csv
    if (fputs(columns.c_str(), file) == EOF) {
        printError("Failed to write header to " + dicoFile);
        fclose(file);
        return 1;
    }

    const int numToAdd = words.size();
    std::atomic<int> numAdded(0);

    // Fetch and print definitions for each word
    // parallelize but do the words in order
#pragma omp parallel for schedule(static, 1)
    for (std::string &word : words) {
        // skip if word starts with "#"
        if (word[0] == '#') {
            continue;
        }

        // convert word to lowercase
        std::transform(word.begin(), word.end(), word.begin(), ::tolower);

        std::string html = fetchHtml(word);
        if (html.empty()) {
            printError("Failed to fetch HTML for word '" + word + "'");
            continue;
        }

        std::string definition = extractDefinition(html);
        if (definition.empty()) {
            printError("Failed to extract definition for word '" + word + "'");
            continue;
        }

        // escape defintion for csv
        std::string escapedDefinition = escapeForCsv(definition);

        // format entry for csv
        std::string entryString =
            "\"" + word + "\",\"" + escapedDefinition + "\"\n";

// write entry to output file
#pragma omp critical(csv)
        {
            if (fputs(entryString.c_str(), file) == EOF) {
                printError("Failed to write entry to " + dicoFile);
                fclose(file);
                exit(1);
            }
            fflush(file);
        }

        numAdded++;
        printProgressBar(numAdded, numToAdd);
    }

    fclose(file);

    std::time_t endTime = std::time(nullptr);

    std::cout << std::endl;
    std::cout << numAdded << " definitions added to " << dicoFile << std::endl;
    std::cout << numToAdd - numAdded << " definitions failed to be added."
              << std::endl;
    std::cout << "Time taken: " << endTime - startTime << " seconds."
              << std::endl;

    return 0;
}