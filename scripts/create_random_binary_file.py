import os
import argparse

def create_random_binary_file(filename, num_bytes):
    """
    Create a binary file with a specified number of random bytes.

    :param filename: The name of the file to be created.
    :param num_bytes: The number of random bytes to write to the file.
    """
    random_bytes = os.urandom(num_bytes)

    with open(filename, 'wb') as file:
        file.write(random_bytes)

    print(f"File '{filename}' created with {num_bytes} random bytes.")

def main():
    # Create argument parser
    parser = argparse.ArgumentParser(description='Generate a file with random binary data.')
    parser.add_argument('filename', type=str, help='Name of the file to create.')
    parser.add_argument('num_bytes', type=int, help='Number of random bytes to write to the file.')

    # Parse arguments
    args = parser.parse_args()

    # Create random binary file with the provided arguments
    create_random_binary_file(args.filename, args.num_bytes)

if __name__ == '__main__':
    main()

