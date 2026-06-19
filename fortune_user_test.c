#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define DEFAULT_DEVICE_PATH "/dev/fortune_device"
#define TEST_FORTUNE "User program reached the kernel driver.\n"
#define BUFFER_SIZE 256

static void print_fortune(const char *label, const char *fortune)
{
	size_t length = strlen(fortune);

	printf("%s%s", label, fortune);
	if (length == 0 || fortune[length - 1] != '\n')
		putchar('\n');
}

static int read_fortune(const char *device_path, char *buffer,
			size_t buffer_size)
{
	int fd;
	ssize_t bytes_read;

	if (buffer_size == 0) {
		fprintf(stderr, "read buffer must not be empty\n");
		return -1;
	}

	fd = open(device_path, O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "open(%s) for read failed: %s\n",
			device_path, strerror(errno));
		return -1;
	}

	bytes_read = read(fd, buffer, buffer_size - 1);
	if (bytes_read < 0) {
		fprintf(stderr, "read(%s) failed: %s\n",
			device_path, strerror(errno));
		close(fd);
		return -1;
	}

	buffer[bytes_read] = '\0';

	if (close(fd) < 0) {
		fprintf(stderr, "close(%s) after read failed: %s\n",
			device_path, strerror(errno));
		return -1;
	}

	return 0;
}

static int write_fortune(const char *device_path, const char *message)
{
	int fd;
	size_t message_length = strlen(message);
	ssize_t bytes_written;

	fd = open(device_path, O_WRONLY);
	if (fd < 0) {
		fprintf(stderr, "open(%s) for write failed: %s\n",
			device_path, strerror(errno));
		return -1;
	}

	bytes_written = write(fd, message, message_length);
	if (bytes_written < 0) {
		fprintf(stderr, "write(%s) failed: %s\n",
			device_path, strerror(errno));
		close(fd);
		return -1;
	}

	if ((size_t)bytes_written != message_length) {
		fprintf(stderr, "short write: wrote %ld of %lu bytes\n",
			(long)bytes_written, (unsigned long)message_length);
		close(fd);
		return -1;
	}

	if (close(fd) < 0) {
		fprintf(stderr, "close(%s) after write failed: %s\n",
			device_path, strerror(errno));
		return -1;
	}

	return 0;
}

int main(int argc, char **argv)
{
	const char *device_path = DEFAULT_DEVICE_PATH;
	char buffer[BUFFER_SIZE];

	if (argc > 2) {
		fprintf(stderr, "usage: %s [device-path]\n", argv[0]);
		return EXIT_FAILURE;
	}

	if (argc == 2)
		device_path = argv[1];

	printf("fortune_user_test: device=%s\n", device_path);

	if (read_fortune(device_path, buffer, sizeof(buffer)) != 0)
		return EXIT_FAILURE;
	print_fortune("initial fortune: ", buffer);

	print_fortune("writing fortune: ", TEST_FORTUNE);
	if (write_fortune(device_path, TEST_FORTUNE) != 0)
		return EXIT_FAILURE;

	if (read_fortune(device_path, buffer, sizeof(buffer)) != 0)
		return EXIT_FAILURE;
	print_fortune("updated fortune: ", buffer);

	if (strcmp(buffer, TEST_FORTUNE) != 0) {
		fprintf(stderr,
			"verification failed: device returned unexpected data\n");
		return EXIT_FAILURE;
	}

	printf("verification: ok\n");
	return EXIT_SUCCESS;
}
