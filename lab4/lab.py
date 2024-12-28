def filter_lines(input_filename="input.txt", output_filename="output.txt", encoding='utf-16-le'):

    prefixes = ["NtAllocate", "NtFree", "Loaded", "NtQuery", "NtClose"]

    try:
        with open(input_filename, 'r', encoding=encoding) as infile, open(output_filename, 'w') as outfile:
            for line in infile:
                for prefix in prefixes:
                    if line.strip().startswith(prefix):
                        outfile.write(line)
                        break  # Move to the next line once a match is found
    except FileNotFoundError:
        print(f"Error: Input file '{input_filename}' not found.")
    except Exception as e:
        print(f"An error occurred: {e}")
    else:
        print(f"Filtered lines written to '{output_filename}'")

input_file = "mckusick_karels.txt"  
output_file = "mckusick_karels2.txt"


filter_lines(input_file, output_file, encoding='utf-16-le')