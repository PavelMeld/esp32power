#include <stdio.h>
#include <stdlib.h>
#include <string.h>


typedef enum { hunk_text, hunk_include, hunk_printf, hunk_program } hunk_t;

void encode_program(FILE *output, unsigned char *data, long start_pos, long end_pos) {
	
	for (long i = start_pos; i < end_pos; i++) {
		if (data[i] != '\r') {
			putc(data[i], output);
		}
	}
	fprintf(output, "\n");
}

void encode_text(FILE *output, unsigned char *data, long start_pos, long end_pos) {
	int chunk_size = 0;
	
	fprintf(output, "\tmemcpy(chunk_data, \"");
	for (long i = start_pos; i < end_pos; i++) {
		if (data[i] == '"') {
			fprintf(output, "%s", "\\\"");
		} else if (data[i] == '\n') {
			fprintf(output, "%s", "\\n");
		} else if (data[i] == '\r') {
			fprintf(output, "%s", "\\r");
		} else if (data[i] == '\\') {
            fprintf(output, "\\\\%c", data[++i]);
		} else if (((data[i] >= ' ') && (data[i] <= 127)) || data[i] == '\t') {
			putc(data[i], output);
		} else {
			fprintf(output, "\\%o", data[i]);
		}

		chunk_size++;	
		
		if ((chunk_size % 512) == 0) {
			fprintf(output, "\", %i);\n", chunk_size);
			fprintf(output, "\thttpd_resp_send_chunk(request, chunk_data, %i);\n", chunk_size);
			fprintf(output, "\tmemcpy(chunk_data, \"");
			chunk_size = 0;
		}
	}
	fprintf(output, "\", %i);\n", chunk_size);

	if (chunk_size) {
		fprintf(output, "\thttpd_resp_send_chunk(request, chunk_data, %i);\n", chunk_size);
	}

}

void encode_printf(FILE *output, unsigned char *data, long start_pos, long end_pos) {
	

	if ((data[start_pos] == ' ') || (data[start_pos] == '\t')) {
		fprintf(output, "\tchunk_size = sprintf(chunk_data, \"%%i\", ");
		fwrite(data+start_pos, end_pos-start_pos, 1, output);
		fprintf(output, ");\n");
	} else {
		long i;
		fprintf(output, "\tchunk_size = sprintf(chunk_data, \"%%");
		for (i = start_pos; i < end_pos; i++) {
			if ((data[i] == ' ') || (data[i] == '\t'))
				break;
			else if (data[i] == '~')
				putc(' ', output);
			else
				putc(data[i], output);
		}
		fprintf(output, "\", ");
		fwrite(data+i, end_pos-i, 1, output);
		fprintf(output, ");\n");
	}

	fprintf(output, "\tif (chunk_size) httpd_resp_send_chunk(request, chunk_data, chunk_size);\n");
}

void process_data(FILE *output, char *data, long start_pos, long end_pos, hunk_t type, bool includes) {
	if (end_pos == start_pos)
		return;
	
	switch (type) {
	case hunk_text:
		if (!includes)
			encode_text(output, (unsigned char *)data, start_pos, end_pos);	
		break;
	case hunk_include: 
		if (includes)
			encode_program(output, (unsigned char *)data, start_pos, end_pos);	
		break;
	case hunk_printf:
		if (!includes)
			encode_printf(output, (unsigned char *)data, start_pos, end_pos);	
		break;
	case hunk_program:
		if (!includes)
			encode_program(output, (unsigned char *)data, start_pos, end_pos);	
		break;
	}

}

bool parse_data(FILE *output, char *data, long size, bool includes) {

	long start_pos = 0;
	long end_pos = 0;
	hunk_t type = hunk_text;

	for (long i = 0; i < size; ) {
		if (i < (size - 1)) {
			
			if ((data[i] == '<') && (data[i+1] == '%')) {
				end_pos = i;
				process_data(output, data, start_pos, end_pos, type, includes);
				type = hunk_printf;

				i = i + 2;
				start_pos = i;
				for ( ;i < size; i++) {
					if (i < (size - 1)) {
						if ((data[i] == '%') && (data[i+1] == '>')) {
							end_pos = i;
							process_data(output, data, start_pos, end_pos, type, includes);
							i = i + 2;
							start_pos = i;
							type = hunk_text;
							break;
						}
					} else {
						fprintf(stderr,"Error: matching %%> not found!\n");
						return false;
					}
				}
			} else if ((data[i] == '<') && (data[i+1] == '#')) {
				end_pos = i;
				process_data(output, data, start_pos, end_pos, type, includes);
				type = hunk_include;

				i = i + 2;
				start_pos = i;
				for ( ;i < size; i++) {
					if (i < (size - 1)) {
						if ((data[i] == '#') && (data[i+1] == '>')) {
							end_pos = i;
							process_data(output, data, start_pos, end_pos, type, includes);
							i = i + 2;
							start_pos = i;
							type = hunk_text;
							break;
						}
					} else {
						fprintf(stderr,"Error: matching #> not found!\n");
						return false;
					}
				}
			} else if ((data[i] == '<') && (data[i+1] == '?')) {
				end_pos = i;
				process_data(output, data, start_pos, end_pos, type, includes);
				type = hunk_program;

				i = i + 2;
				start_pos = i;
				for ( ;i < size; i++) {
					if (i < (size - 1)) {
						if ((data[i] == '?') && (data[i+1] == '>')) {
							end_pos = i;
							process_data(output, data, start_pos, end_pos, type, includes);
							i = i + 2;
							start_pos = i;
							type = hunk_text;
							break;
						}
					} else {
						fprintf(stderr,"Error: matching ?> not found!\n");
						return false;
					}
				}
			} else
				i++;
		} else 
			i++;
	}
	end_pos = size;
	process_data(output, data, start_pos, end_pos, type, includes);

	return true;
}

char *make_id(const char *name) {

	const char *q = strrchr(name, '\\');
	if (!q)
		q = strrchr(name, '/');
	if (q)
		q++;
	else
		q = (char *)name;

	char *buf = strdup(q);
	char *p = buf;


	while (*p) {
		if (*p >= 'a' && *p <= 'z') {
			*p += 'A' - 'a';
		} else if (*p >= 'A' && *p <= 'Z') {
		} else if (*p >= '0' && *p <= '9') {
		} else {
			*p = '_';
		}
		p++;
	}
	return buf;
}

void print_usage() {
	fprintf(stderr, "Usage: csp [-v2] [-o outfile] ([-l listfiles ...]|infile)\n");
}

int main(int argc, char* argv[])
{
	char *input_name = NULL;
	char *output_name = NULL;
	FILE *input;
	FILE *output;
	enum {mode_file, mode_list} mode = mode_file;
	int list_from;

	/* Разбор параметров */
	if (argc < 2) {
		print_usage();
		return 1;
	}
	
	for (int narg = 1; narg < argc; narg++) {
		if (strcmp(argv[narg], "-o") == 0) {
			if (narg+1 >= argc) {
				print_usage();
				return 1;
			}
			output_name = argv[++narg];
		}  else if (strcmp(argv[narg], "-l") == 0) {
			mode = mode_list;
			if (narg+1 >= argc) {
				print_usage();
				return 1;
			}
			list_from = narg+1;
			break;
		} else {
			if (input_name) {
				print_usage();
				return 1;
			}
			input_name = argv[narg];
		}
	}
	
	if (mode == mode_list) {
		/* Запись в выходной файл */
		if (output_name) {
			output = fopen(output_name, "wt");
			if (output == NULL) {
				fprintf(stderr, "Could not open file: %s\n", output_name);
				return 1;
			}
		} else {
			output = stdout;
		}
		fprintf(output, "#ifndef SERVED_PAGES_H\n");
		fprintf(output, "#define SERVED_PAGES_H\n");

		fprintf(output, "#include <esp_http_server.h>\n\n");

		for (int nfile = list_from; nfile < argc; nfile++) {
			char *func_name = make_id(argv[nfile]);
			fprintf(output, "extern void serve_page_webs_%s(httpd_req_t *request);\n", func_name);
			free(func_name);
		}


		fprintf(output, "\n#endif\n");
		
	} else {
		bool is_csp = true;
		if (NULL == strstr(input_name, ".csp"))
			is_csp = false;

		/* Чтение исходного файла */
		input = fopen(input_name, "rb");
		if (input == NULL) {
			fprintf(stderr, "Could not open file: %s\n", input_name);
			return 1;
		}

		fseek(input, 0, SEEK_END);
		long file_size = ftell(input);
		rewind(input);

		char *file_data = (char *)malloc(file_size);
		fread(file_data, file_size, 1, input);
		fclose(input);

		
		/* Запись в выходной файл */
		if (output_name) {
			output = fopen(output_name, "wt");
			if (output == NULL) {
				fprintf(stderr, "Could not open file: %s\n", output_name);
				return 1;
			}
		} else {
			output = stdout;
		}
		
		fprintf(output, "#include <esp_http_server.h>\n\n");
		fprintf(output, "static char chunk_data[512];\n");

		if (is_csp)
		{
			if (!parse_data(output, file_data, file_size, true)) {
				fprintf(stderr, "Errors encountered, exiting...\n");
				if (output != stdout)
					fclose(output);
				return 1;
			}
		}
		else
		{
			fprintf(output, "int len = %d;\n", file_size);
			fprintf(output, "char bin[] = {");

			for (int i = 0; i < (file_size - 1); ++i)
			{
				fprintf(output, "0x%x, ", (unsigned char)file_data[i]);				
			}
			fprintf(output, "0x%x};\n", (unsigned char)file_data[file_size - 1]);				
		}

		char *func_name = make_id(input_name);
		fprintf(output, "\nesp_err_t serve_page_webs_%s(httpd_req_t *  request) {\n", func_name);
		free(func_name);
		fprintf(output, "\tint chunk_size;\n");

		if (is_csp)
		{

			if (!parse_data(output, file_data, file_size, false)) {
				fprintf(stderr, "Errors encountered, exiting...\n");
				if (output != stdout)
					fclose(output);
				return 1;
			}
		}
		else
		{
			fprintf(output, "	netconn_write(conn->conn, (void *)bin, len, NETCONN_COPY);\n");
		}

		fprintf(output, "\thttpd_resp_send_chunk(request, NULL, 0);\n");
		fprintf(output, "\treturn ESP_OK;\n");
		fprintf(output, "}\n");
	
		if (output != stdout)
			fclose(output);
	}

	return 0;
}
