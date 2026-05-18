NAME        = f1_telemetry

CC          = clang++
CXXFLAGS    = -Wall -Wextra -std=c++20 -I./include -I./imgui -I./imgui/backends
LDFLAGS     = -lglfw -lvulkan -ldl -lpthread

SRC_DIR     = src
OBJ_DIR     = obj


IMGUI_SRC   = imgui/imgui.cpp \
              imgui/imgui_draw.cpp \
              imgui/imgui_tables.cpp \
              imgui/imgui_widgets.cpp \
              imgui/backends/imgui_impl_glfw.cpp \
              imgui/backends/imgui_impl_vulkan.cpp

SRC         = $(SRC_DIR)/main.cpp \
              $(SRC_DIR)/DataLoader.cpp \
              $(IMGUI_SRC)

OBJ         = $(patsubst %.cpp, $(OBJ_DIR)/%.o, $(SRC))

all: $(NAME)

$(NAME): $(OBJ)
	$(CC) $(OBJ) -o $(NAME) $(LDFLAGS)

$(OBJ_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CC) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re