import requests
from bs4 import BeautifulSoup
import sys
import csv

baseUrl = "https://www.wordreference.com/definition/"

corpusFile = "corpus.txt"
data = []
dicoFile = "dico.csv"

# read from corpus
with open(corpusFile, mode='r', encoding='utf-8') as corpus:
    lines = [line.strip() for line in corpus.readlines()]
    # remove empty lines
    lines = [line for line in lines if line]

words = lines

numToAdd = len(words)
numAdded = 0

print(f"Adding {numToAdd} words to {dicoFile}")

for word in words:

    # get HTML
    fullUrl = baseUrl + word
    response = requests.get(fullUrl)

    soup = BeautifulSoup(response.text, "html.parser")

    try:
        defSpan = soup.find("span", {"class": "rh_def"})
        defText = defSpan.find(string=True, recursive=False)
        # remove last character (period or colon)
        defText = defText[:-1]
        # remove extra spaces and newlines and uppercase
        defText = defText.strip().lower()
        # print(defText)
        data.append([word, defText])
        numAdded += 1
        print(f"{numAdded}/{numToAdd}")
    except AttributeError:
        print(f"Word '{word}' not found in WordReference.")
        continue

# write to CSV
with open(dicoFile, mode='w', newline='', encoding='utf-8') as file:
    writer = csv.writer(file)
    writer.writerow(["word", "definition"])
    writer.writerows(data)

print(f"Added {numAdded}/{numToAdd} words to {dicoFile}")
