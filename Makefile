# ══════════════════════════════════════════════════════════════════════
# Webserv Makefile
# C++98, -Wall -Wextra -Werror
# ══════════════════════════════════════════════════════════════════════

NAME    := webserv
CXX     := c++
CXXFLAGS := -Wall -Wextra -Werror -std=c++98

INCLUDE := -I Network_Server -I utils -I Http

SRCDIR  := .
OBJDIR  := obj

SRCS    :=  main.cpp                      \
            Network_Server/Socketbinder.cpp   \
            Network_Server/Pollreactor.cpp    \
            Network_Server/Reactorbridge.cpp  \
            Network_Server/Signalguard.cpp    \
            Network_Server/Signals.cpp        \
            utils/ft_memset.cpp \
			Http/HttpParser.cpp \
			Http/HttpRequest.cpp \
			Http/HttpRequestHandler.cpp \
			Http/HttpResponse.cpp \
			Http/MimeTypes.cpp \
			Http/StatusCode.cpp \
			Config/ConfigParser.cpp \
			Router/Router.cpp \
			Http/StaticHandler.cpp \
			Http/AutoIndex.cpp \
			Cgi/CgiEnv.cpp \
			Cgi/CgiProcess.cpp \
			Http/CgiHandler.cpp \

OBJS    := $(patsubst %.cpp, $(OBJDIR)/%.o, $(notdir $(SRCS)))

# ── Renkler (isteğe bağlı, terminalde güzel görünür) ─────────────────
GREEN  := \033[0;32m
RESET  := \033[0m

# ── Hedefler ─────────────────────────────────────────────────────────

all: $(NAME)

$(NAME): $(OBJS)
	@$(CXX) $(CXXFLAGS) $(OBJS) -o $(NAME)
	@echo "$(GREEN)✓ $(NAME) derlendi$(RESET)"

$(OBJDIR)/main.o: main.cpp | $(OBJDIR)
	@$(CXX) $(CXXFLAGS) $(INCLUDE) -c $< -o $@
	@echo "  CC $<"

$(OBJDIR)/%.o: Network_Server/%.cpp | $(OBJDIR)
	@$(CXX) $(CXXFLAGS) $(INCLUDE) -c $< -o $@
	@echo "  CC $<"

$(OBJDIR)/%.o: utils/%.cpp | $(OBJDIR)
	@$(CXX) $(CXXFLAGS) $(INCLUDE) -c $< -o $@
	@echo "  CC $<"

$(OBJDIR)/%.o: Http/%.cpp | $(OBJDIR)
	@$(CXX) $(CXXFLAGS) $(INCLUDE) -c $< -o $@
	@echo "  CC $<"

$(OBJDIR)/%.o: Config/%.cpp | $(OBJDIR)
	@$(CXX) $(CXXFLAGS) $(INCLUDE) -c $< -o $@
	@echo "  CC $<"

$(OBJDIR)/%.o: Router/%.cpp | $(OBJDIR)
	@$(CXX) $(CXXFLAGS) $(INCLUDE) -c $< -o $@

$(OBJDIR)/%.o: Cgi/%.cpp | $(OBJDIR)
	@$(CXX) $(CXXFLAGS) $(INCLUDE) -c $< -o $@

$(OBJDIR):
	@mkdir -p $(OBJDIR)

clean:
	@rm -rf $(OBJDIR)
	@echo "$(GREEN)✓ obj/ temizlendi$(RESET)"

fclean: clean
	@rm -f $(NAME)
	@echo "$(GREEN)✓ $(NAME) silindi$(RESET)"

re: fclean all

# Yeniden linklemeyi önle: header değişince sadece bağımlı .o yeniden derlenir
$(OBJDIR)/main.o:          Network_Server/Pollreactor.hpp Network_Server/Reactorbridge.hpp Http/HttpRequestHandler.hpp
$(OBJDIR)/Socketbinder.o:  Network_Server/Socketbinder.hpp Network_Server/Nettypes.hpp
$(OBJDIR)/Pollreactor.o:   Network_Server/Pollreactor.hpp Network_Server/Connectionslot.hpp
$(OBJDIR)/Reactorbridge.o: Network_Server/Reactorbridge.hpp Network_Server/Pollreactor.hpp Network_Server/Connectionslot.hpp
$(OBJDIR)/Signalguard.o:   Network_Server/Signalguard.hpp
$(OBJDIR)/ft_memset.o:     utils/Utils.hpp
$(OBJDIR)/HttpParser.o:    Http/HttpParser.hpp Http/HttpRequest.hpp
$(OBJDIR)/HttpRequest.o:   Http/HttpRequest.hpp
$(OBJDIR)/HttpRequestHandler.o: Http/HttpRequestHandler.hpp Http/HttpRequest.hpp Http/HttpParser.hpp Http/CgiHandler.hpp Network_Server/Connectionslot.hpp
$(OBJDIR)/StaticHandler.o: Http/StaticHandler.hpp Http/HttpRequest.hpp
$(OBJDIR)/CgiHandler.o:    Http/CgiHandler.hpp Cgi/CgiSession.hpp Cgi/CgiProcess.hpp Http/HttpRequest.hpp
$(OBJDIR)/CgiProcess.o:    Cgi/CgiProcess.hpp
$(OBJDIR)/CgiEnv.o:        Cgi/CgiEnv.hpp Http/HttpRequest.hpp

.PHONY: all clean fclean re
