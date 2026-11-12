#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INPUT_SIZE 256
#define HISTORY_FILE "calculator_history.txt"
#define HISTORY_LINE_SIZE 512
#define PI 3.14159265358979323846

typedef enum {
    MODE_STANDARD = 1,
    MODE_SCIENTIFIC = 2
} CalculatorMode;

typedef enum {
    ANGLE_RADIANS = 1,
    ANGLE_DEGREES = 2
} AngleMode;

typedef struct {
    const char *text;
    size_t position;
    double memory;
    AngleMode angle_mode;
    char error[128];
} Parser;

static void set_error(Parser *parser, const char *message)
{
    if (parser->error[0] == '\0') {
        snprintf(parser->error, sizeof(parser->error), "%s", message);
    }
}

static void skip_spaces(Parser *parser)
{
    while (isspace((unsigned char)parser->text[parser->position])) {
        parser->position++;
    }
}

static double to_radians(double value, AngleMode angle_mode)
{
    return angle_mode == ANGLE_DEGREES ? value * PI / 180.0 : value;
}

static double from_radians(double value, AngleMode angle_mode)
{
    return angle_mode == ANGLE_DEGREES ? value * 180.0 / PI : value;
}

static double parse_expression(Parser *parser);
static double parse_unary(Parser *parser);

static double parse_primary(Parser *parser)
{
    char identifier[32];
    size_t length = 0;
    char *end;
    double value;

    skip_spaces(parser);
    if (parser->text[parser->position] == '(') {
        parser->position++;
        value = parse_expression(parser);
        skip_spaces(parser);
        if (parser->text[parser->position] != ')') {
            set_error(parser, "Missing closing parenthesis.");
            return 0.0;
        }
        parser->position++;
        return value;
    }

    if (isalpha((unsigned char)parser->text[parser->position])) {
        while (isalpha((unsigned char)parser->text[parser->position]) &&
               length < sizeof(identifier) - 1) {
            identifier[length++] =
                (char)tolower((unsigned char)parser->text[parser->position++]);
        }
        identifier[length] = '\0';

        if (strcmp(identifier, "pi") == 0) {
            return PI;
        }
        if (strcmp(identifier, "e") == 0) {
            return exp(1.0);
        }
        if (strcmp(identifier, "ans") == 0) {
            return parser->memory;
        }

        skip_spaces(parser);
        if (parser->text[parser->position] != '(') {
            set_error(parser, "Unknown name. Use pi, e, ans, or a supported function.");
            return 0.0;
        }
        parser->position++;
        value = parse_expression(parser);
        skip_spaces(parser);
        if (parser->text[parser->position] != ')') {
            set_error(parser, "Missing closing parenthesis.");
            return 0.0;
        }
        parser->position++;

        if (strcmp(identifier, "sin") == 0) {
            return sin(to_radians(value, parser->angle_mode));
        }
        if (strcmp(identifier, "cos") == 0) {
            return cos(to_radians(value, parser->angle_mode));
        }
        if (strcmp(identifier, "tan") == 0) {
            return tan(to_radians(value, parser->angle_mode));
        }
        if (strcmp(identifier, "asin") == 0) {
            if (value < -1.0 || value > 1.0) {
                set_error(parser, "asin input must be between -1 and 1.");
                return 0.0;
            }
            return from_radians(asin(value), parser->angle_mode);
        }
        if (strcmp(identifier, "acos") == 0) {
            if (value < -1.0 || value > 1.0) {
                set_error(parser, "acos input must be between -1 and 1.");
                return 0.0;
            }
            return from_radians(acos(value), parser->angle_mode);
        }
        if (strcmp(identifier, "atan") == 0) {
            return from_radians(atan(value), parser->angle_mode);
        }
        if (strcmp(identifier, "sqrt") == 0) {
            if (value < 0.0) {
                set_error(parser, "sqrt input cannot be negative.");
                return 0.0;
            }
            return sqrt(value);
        }
        if (strcmp(identifier, "cbrt") == 0) {
            return cbrt(value);
        }
        if (strcmp(identifier, "ln") == 0) {
            if (value <= 0.0) {
                set_error(parser, "ln input must be greater than zero.");
                return 0.0;
            }
            return log(value);
        }
        if (strcmp(identifier, "log") == 0) {
            if (value <= 0.0) {
                set_error(parser, "log input must be greater than zero.");
                return 0.0;
            }
            return log10(value);
        }
        if (strcmp(identifier, "exp") == 0) {
            return exp(value);
        }
        if (strcmp(identifier, "abs") == 0) {
            return fabs(value);
        }
        if (strcmp(identifier, "floor") == 0) {
            return floor(value);
        }
        if (strcmp(identifier, "ceil") == 0) {
            return ceil(value);
        }

        set_error(parser, "Unknown function.");
        return 0.0;
    }

    errno = 0;
    value = strtod(parser->text + parser->position, &end);
    if (end == parser->text + parser->position) {
        set_error(parser, "Expected a number, constant, or function.");
        return 0.0;
    }
    if (errno == ERANGE) {
        set_error(parser, "Number is outside the supported range.");
    }
    parser->position = (size_t)(end - parser->text);
    return value;
}

static double parse_postfix(Parser *parser)
{
    double value = parse_primary(parser);

    while (parser->error[0] == '\0') {
        skip_spaces(parser);
        if (parser->text[parser->position] != '!') {
            break;
        }
        parser->position++;
        if (value < 0.0 || value > 170.0 || floor(value) != value) {
            set_error(parser, "Factorial needs a whole number from 0 to 170.");
            return 0.0;
        }
        value = tgamma(value + 1.0);
    }
    return value;
}

static double parse_power(Parser *parser)
{
    double left = parse_postfix(parser);

    skip_spaces(parser);
    if (parser->text[parser->position] == '^') {
        parser->position++;
        left = pow(left, parse_unary(parser));
    }
    return left;
}

static double parse_unary(Parser *parser)
{
    double value;

    skip_spaces(parser);
    if (parser->text[parser->position] == '+') {
        parser->position++;
        return parse_unary(parser);
    }
    if (parser->text[parser->position] == '-') {
        parser->position++;
        return -parse_unary(parser);
    }
    value = parse_power(parser);
    return value;
}

static double parse_term(Parser *parser)
{
    double value = parse_unary(parser);

    while (parser->error[0] == '\0') {
        char operator;
        double right;

        skip_spaces(parser);
        operator = parser->text[parser->position];
        if (operator != '*' && operator != '/') {
            break;
        }
        parser->position++;
        right = parse_unary(parser);
        if (operator == '*' ) {
            value *= right;
        } else {
            if (right == 0.0) {
                set_error(parser, "Division by zero is not allowed.");
                return 0.0;
            }
            value /= right;
        }
    }
    return value;
}

static double parse_expression(Parser *parser)
{
    double value = parse_term(parser);

    while (parser->error[0] == '\0') {
        char operator;
        double right;

        skip_spaces(parser);
        operator = parser->text[parser->position];
        if (operator != '+' && operator != '-') {
            break;
        }
        parser->position++;
        right = parse_term(parser);
        value = operator == '+' ? value + right : value - right;
    }
    return value;
}

static int evaluate(const char *text, double memory, AngleMode angle_mode,
                    double *result, char *error, size_t error_size)
{
    Parser parser = {text, 0, memory, angle_mode, ""};

    *result = parse_expression(&parser);
    skip_spaces(&parser);
    if (parser.error[0] == '\0' && parser.text[parser.position] != '\0') {
        set_error(&parser, "Unexpected characters after the expression.");
    }
    if (parser.error[0] == '\0' && !isfinite(*result)) {
        set_error(&parser, "The result is not a finite number.");
    }
    if (parser.error[0] != '\0') {
        snprintf(error, error_size, "%s", parser.error);
        return 0;
    }
    return 1;
}

static void save_history(const char *expression, double result)
{
    FILE *file = fopen(HISTORY_FILE, "a");
    if (file == NULL) {
        printf("  Note: history could not be saved.\n");
        return;
    }
    fprintf(file, "%s = %.15g\n", expression, result);
    fclose(file);
}

static void show_history(void)
{
    FILE *file = fopen(HISTORY_FILE, "r");
    char line[HISTORY_LINE_SIZE];
    int count = 0;

    if (file == NULL) {
        printf("\n  No saved calculations yet.\n");
        return;
    }
    printf("\n  ---------------- HISTORY ----------------\n");
    while (fgets(line, sizeof(line), file) != NULL) {
        printf("  %s", line);
        count++;
    }
    if (count == 0) {
        printf("  No saved calculations yet.\n");
    }
    printf("  ------------------------------------------\n");
    fclose(file);
}

static void clear_history(void)
{
    FILE *file = fopen(HISTORY_FILE, "w");
    if (file == NULL) {
        printf("  Could not clear history.\n");
        return;
    }
    fclose(file);
    printf("  History cleared.\n");
}

static void print_help(void)
{
    printf("\n  EXPRESSION EXAMPLES\n");
    printf("  2 + 3 * 4       (2 + 3) / 4       2^8       5!\n");
    printf("  sin(30)         sqrt(81)          log(100)  ln(e)\n");
    printf("  abs(-12.5)      asin(1)           pi * 2    ans + 5\n");
    printf("\n  COMMANDS\n");
    printf("  m+ / m-         Add or subtract the displayed result from memory\n");
    printf("  mr              Recall memory     mc              Clear memory\n");
    printf("  mode            Switch Standard / Scientific mode\n");
    printf("  angle           Switch Degrees / Radians\n");
    printf("  h               Show saved history     hc              Clear history\n");
    printf("  help            Show this help         q               Quit\n");
}

static void print_header(CalculatorMode mode, AngleMode angle_mode,
                         double memory, double answer)
{
    printf("\n============================================================\n");
    printf("                 SIMPLE SCIENTIFIC CALCULATOR\n");
    printf("============================================================\n");
    printf("  Mode: %-11s Angle: %-7s Memory: %-12.6g\n",
           mode == MODE_SCIENTIFIC ? "Scientific" : "Standard",
           angle_mode == ANGLE_DEGREES ? "Degrees" : "Radians", memory);
    printf("  Answer: %.15g\n", answer);
    printf("------------------------------------------------------------\n");
}

int main(void)
{
    char input[INPUT_SIZE];
    char error[128];
    double answer = 0.0;
    double memory = 0.0;
    CalculatorMode mode = MODE_SCIENTIFIC;
    AngleMode angle_mode = ANGLE_DEGREES;

    printf("Welcome. Type an expression, 'help' for commands, or 'q' to quit.\n");
    for (;;) {
        double result;
        size_t length;

        print_header(mode, angle_mode, memory, answer);
        printf("  > ");
        if (fgets(input, sizeof(input), stdin) == NULL) {
            break;
        }
        length = strlen(input);
        while (length > 0 && (input[length - 1] == '\n' ||
                              input[length - 1] == '\r')) {
            input[--length] = '\0';
        }
        if (length == 0) {
            continue;
        }
        if (strcmp(input, "q") == 0 || strcmp(input, "quit") == 0) {
            break;
        }
        if (strcmp(input, "help") == 0) {
            print_help();
            continue;
        }
        if (strcmp(input, "h") == 0 || strcmp(input, "history") == 0) {
            show_history();
            continue;
        }
        if (strcmp(input, "hc") == 0) {
            clear_history();
            continue;
        }
        if (strcmp(input, "mode") == 0) {
            mode = mode == MODE_STANDARD ? MODE_SCIENTIFIC : MODE_STANDARD;
            printf("  Mode changed to %s.\n",
                   mode == MODE_SCIENTIFIC ? "Scientific" : "Standard");
            continue;
        }
        if (strcmp(input, "angle") == 0) {
            angle_mode = angle_mode == ANGLE_DEGREES
                             ? ANGLE_RADIANS : ANGLE_DEGREES;
            printf("  Angle mode changed to %s.\n",
                   angle_mode == ANGLE_DEGREES ? "Degrees" : "Radians");
            continue;
        }
        if (strcmp(input, "mr") == 0) {
            answer = memory;
            printf("  Memory recalled: %.15g\n", answer);
            continue;
        }
        if (strcmp(input, "mc") == 0) {
            memory = 0.0;
            printf("  Memory cleared.\n");
            continue;
        }
        if (strcmp(input, "m+") == 0 || strcmp(input, "m-") == 0) {
            memory += strcmp(input, "m+") == 0 ? answer : -answer;
            printf("  Memory: %.15g\n", memory);
            continue;
        }

        if (!evaluate(input, memory, angle_mode, &result, error, sizeof(error))) {
            printf("  Error: %s\n", error);
            continue;
        }
        answer = result;
        save_history(input, result);
        printf("  Result: %.15g\n", result);
    }

    printf("\nThanks for using the calculator. Goodbye!\n");
    return 0;
}
