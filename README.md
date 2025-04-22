# immorthon

An LLM-based definition generator

## Dataset

The training data is a collection of word/definition pairs. Several corpuses of word have been tried:

- [3000 most common words in English](https://www.ef.com/wwen/english-resources/english-vocabulary/top-3000-words/)
- [MIT's 10000-word list](https://www.mit.edu/~ecprice/wordlist.10000)

The dataset's format is a CSV file with the following columns:

- `word`: the word
- `definition`: the definition of the word

The definitions are scraped from the [Oxford Learner's Dictionaries](https://www.oxfordlearnersdictionaries.com/).

### Scraping dataset

The scraping script is located in the [`main.cpp`](main.cpp) file which can be compiled and run with the following command:

```sh
make && ./main <corpus_file> <output_file>
```

where `<corpus_file>` is the path to a file containing alist of words and
`<output_file>` is the path to the created file which will contain the definitions scraped.

If a given word is not available in the dictionnary, its definition will not be added to `<output_file>` and a message will be printed to the console.

### Parallel Computing

The code uses OpenMP to parallelize the scraping process. You can disable parallel computing by building the code with

```sh
make nooomp
```

This is significantly slower, but can be useful if a word causes you trouble and you need to debug it.
