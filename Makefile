NAME        = f1_telemetry

CC          = clang++
CXXFLAGS    = -Wall -Wextra -Werror -std=c++20 -I./include

SRC_DIR     = src
OBJ_DIR     = obj

SRC         = $(SRC_DIR)/main.cpp \

OBJ         = $(patsubst $(SRC_DIR)/%.cpp, $(OBJ_DIR)/%.o, $(SRC))

all: $(NAME)

$(NAME): $(OBJ)
	$(CC) $(OBJ) -o $(NAME)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re