def extract_unique_prefixes(input_filename, output_filename="output.txt", encoding='utf-16-le'):
    """
    Извлекает и записывает в файл все уникальные префиксы из входного файла.

    Префикс - это часть строки до первой открывающей круглой скобки '('.
    """
    unique_prefixes = set()

    try:
        for i in input_filename:
            with open(i, 'r', encoding=encoding) as infile:
                for line in infile:
                    line = line.strip()
                    if '(' in line:
                        prefix = line.split('(', 1)[0]
                        unique_prefixes.add(prefix)

        with open(output_filename, 'w') as outfile:
            for prefix in sorted(unique_prefixes):  # Сортировка для удобства
                outfile.write(prefix + '\n')

    except FileNotFoundError:
        print(f"Error: Input file '{input_filename}' not found.")
    except Exception as e:
        print(f"An error occurred: {e}")
    else:
        print(f"Unique prefixes written to '{output_filename}'")

input_file = ["main.txt", "buddy.txt", "mckusick_karels.txt"]
output_file = "main2.txt"

extract_unique_prefixes(input_file, output_file, encoding='utf-16-le')