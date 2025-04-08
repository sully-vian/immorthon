#include <curl/curl.h>
#include <format>
#include <fstream>
#include <iostream>
#include <libxml/HTMLparser.h>
#include <libxml/xpath.h>
#include <omp.h>
#include <vector>

const std::string columns = "word,definition\n";
const std::string baseUrl = "https://www.wordreference.com/definition/";
const std::string corpusFile = "corpus.txt";
const std::string dicoFile = "dico-c.csv";

const xmlChar *defXpathExpr = (const xmlChar *)"//span[@class='rh_def']";
const xmlChar *sdefXpathExpr = (const xmlChar *)"//span[@class='rh_sdef']";

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
        std::cerr << "Failed to parse HTML." << std::endl;
        return "";
    }

    // std::cout << html << std::endl;

    // Create XPath context
    xmlXPathContextPtr xpathCtx = xmlXPathNewContext(doc);
    if (!xpathCtx) {
        std::cerr << "Failed to create XPath context." << std::endl;
        xmlFreeDoc(doc);
        return "";
    }

    xmlXPathObjectPtr xpathObj;

    // first get / check if there is a <span class='rh_sdef'>
    xpathObj = xmlXPathEvalExpression(sdefXpathExpr, xpathCtx);
    if (!xpathObj || !xpathObj->nodesetval ||
        xpathObj->nodesetval->nodeNr == 0) {
        // try with <span class='rh_def'>
        xpathObj = xmlXPathEvalExpression(defXpathExpr, xpathCtx);
        if (!xpathObj || !xpathObj->nodesetval ||
            xpathObj->nodesetval->nodeNr == 0) {
            // std::cerr << "Failed to evaluate XPath expression." << std::endl;
            xmlXPathFreeObject(xpathObj);
            xmlXPathFreeContext(xpathCtx);
            xmlFreeDoc(doc);
            return "";
        }
    }

    // Extract the first matching node
    xmlNodePtr node = xpathObj->nodesetval->nodeTab[0];
    std::string definition;

    char *content = (char *)xmlNodeGetContent(node);
    if (content) {
        xmlFree(content);
    }

    // Iterate over the children of the node to extract only the direct text
    for (xmlNodePtr child = node->children; child; child = child->next) {
        if (child->type == XML_TEXT_NODE) {
            definition += (const char *)child->content;
        }
    }

    // Cleanup
    xmlXPathFreeObject(xpathObj);
    xmlXPathFreeContext(xpathCtx);
    xmlFreeDoc(doc);

    // Trim and remove last char
    definition.erase(definition.find_last_not_of(" \n\r\t") + 1);
    definition.pop_back();
    return definition;
}

// Function to fetch HTML for a given word
std::string fetchHtml(const std::string &word) {
    const std::string fullUrl = baseUrl + word;

    CURL *curl = curl_easy_init();
    if (!curl) {
        std::cerr << "Failed to initialize curl" << std::endl;
        return "";
    }

    std::string html;
    curl_easy_setopt(curl, CURLOPT_URL, fullUrl.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &html);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        std::cerr << "curl_easy_perform() failed for word '" << word
                  << "': " << curl_easy_strerror(res) << std::endl;
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
        std::cerr << "Failed to open corpus file: " << filename << std::endl;
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

int main() {

    // Read words from the corpus file
    std::vector<std::string> words = readCorpus(corpusFile);
    if (words.empty()) {
        std::cerr << "No words found in the corpus file." << std::endl;
        return 1;
    }

    std::cout << "Fetching definitions for " << words.size() << " words..."
              << std::endl;

    // open dico.csv
    FILE *file = fopen(dicoFile.c_str(), "w");
    if (!file) {
        std::cerr << "Failed to open dico.csv for writing." << std::endl;
        return 1;
    }

    // write header to dico.csv
    if (fputs(columns.c_str(), file) == EOF) {
        std::cerr << "Failed to write header to dico.csv." << std::endl;
        fclose(file);
        return 1;
    }

    const int numToAdd = words.size();
    int numAdded = 0;

    // Fetch and print definitions for each word
#pragma omp parallel for
    for (const std::string &word : words) {
        // skip if word starts with "#"
        if (word[0] == '#') {
            continue;
        }

        std::string html = fetchHtml(word);
        if (html.empty()) {
            std::cerr << "Failed to fetch HTML for word: " << word << std::endl;
            continue;
        }

        std::string definition = extractDefinition(html);
        if (definition.empty()) {
#pragma omp critical(err)
            std::cout << "Word '" << word << "' not found in WordReference."
                      << std::endl;
            continue;
        }

        // format entry for csv
        char buffer[1024];
        snprintf(buffer, sizeof(buffer), "\"%s\",\"%s\"\n", word.c_str(),
                 definition.c_str());
        std::string entryString(buffer);

// write entry to dico.csv
#pragma omp critical(csv)
        {
            if (fputs(entryString.c_str(), file) == EOF) {
                std::cerr << "Failed to write to dico.csv." << std::endl;
                fclose(file);
                exit(1);
            }
            fflush(file);
        }

#pragma omp critical(progress)
        {
            numAdded++;
            std::cout << std::format("{}/{}\n", numAdded, numToAdd);
        }
    }

    fclose(file);

    std::cout << "Definitions saved to " << dicoFile << std::endl;
    std::cout << "Done!" << std::endl;

    return 0;
}