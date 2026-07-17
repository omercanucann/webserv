# ══════════════════════════════════════════════════════════════════════
# Webserv Makefile
# C++98, -Wall -Wextra -Werror
# ══════════════════════════════════════════════════════════════════════

NAME    := webserv
CXX     := c++
CXXFLAGS := -Wall -Wextra -Werror -std=c++98

INCLUDE := -I Network_Server -I utils \
           -I Http/Message -I Http/Protocol -I Http/Handler -I Http/Support

SRCDIR  := .
OBJDIR  := obj

SRCS    :=  main.cpp                      \
            Network_Server/Socketbinder.cpp   \
            Network_Server/Pollreactor.cpp    \
            Network_Server/Reactorbridge.cpp  \
            Network_Server/Signalguard.cpp    \
            Network_Server/Signals.cpp        \
			utils/FileUtils.cpp \
			utils/StringUtils.cpp \
			Http/Protocol/HttpParser.cpp \
			Http/Protocol/HttpFraming.cpp \
			Http/Protocol/RoutePolicy.cpp \
			Http/Protocol/RoutePath.cpp \
			Http/Message/HttpRequest.cpp \
			Http/Message/HttpResponse.cpp \
			Http/Message/StatusCode.cpp \
			Config/ConfigParser.cpp \
			Router/Router.cpp \
			Http/Handler/HttpRequestHandler.cpp \
			Http/Handler/HttpRequestHandlerPreflight.cpp \
			Http/Handler/HttpRequestHandlerResponse.cpp \
			Http/Handler/HttpRequestHandlerCgi.cpp \
			Http/Handler/StaticHandler.cpp \
			Http/Handler/CgiHandler.cpp \
			Http/Support/AutoIndex.cpp \
			Http/Support/MimeTypes.cpp \
			Cgi/CgiEnv.cpp \
			Cgi/CgiProcess.cpp \

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

$(OBJDIR)/%.o: Http/Message/%.cpp | $(OBJDIR)
	@$(CXX) $(CXXFLAGS) $(INCLUDE) -c $< -o $@
	@echo "  CC $<"

$(OBJDIR)/%.o: Http/Protocol/%.cpp | $(OBJDIR)
	@$(CXX) $(CXXFLAGS) $(INCLUDE) -c $< -o $@
	@echo "  CC $<"

$(OBJDIR)/%.o: Http/Handler/%.cpp | $(OBJDIR)
	@$(CXX) $(CXXFLAGS) $(INCLUDE) -c $< -o $@
	@echo "  CC $<"

$(OBJDIR)/%.o: Http/Support/%.cpp | $(OBJDIR)
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
$(OBJDIR)/main.o:          Network_Server/Pollreactor.hpp Network_Server/Reactorbridge.hpp Http/Handler/HttpRequestHandler.hpp
$(OBJDIR)/Socketbinder.o:  Network_Server/Socketbinder.hpp Network_Server/Nettypes.hpp
$(OBJDIR)/Pollreactor.o:   Network_Server/Pollreactor.hpp Network_Server/Connectionslot.hpp
$(OBJDIR)/Reactorbridge.o: Network_Server/Reactorbridge.hpp Network_Server/Pollreactor.hpp Network_Server/Connectionslot.hpp
$(OBJDIR)/Signalguard.o:   Network_Server/Signalguard.hpp
$(OBJDIR)/FileUtils.o:     utils/FileUtils.hpp
$(OBJDIR)/StringUtils.o:   utils/StringUtils.hpp
$(OBJDIR)/HttpParser.o:    Http/Protocol/HttpParser.hpp Http/Message/HttpRequest.hpp utils/StringUtils.hpp
$(OBJDIR)/HttpFraming.o:   Http/Protocol/HttpFraming.hpp utils/StringUtils.hpp
$(OBJDIR)/HttpRequest.o:   Http/Message/HttpRequest.hpp utils/StringUtils.hpp
$(OBJDIR)/HttpResponse.o:  Http/Message/HttpResponse.hpp Http/Message/StatusCode.hpp utils/StringUtils.hpp
$(OBJDIR)/MimeTypes.o:     Http/Support/MimeTypes.hpp utils/StringUtils.hpp
$(OBJDIR)/HttpRequestHandler.o: Http/Handler/HttpRequestHandler.hpp Http/Message/HttpRequest.hpp Http/Protocol/HttpParser.hpp Http/Handler/CgiHandler.hpp Network_Server/Connectionslot.hpp
$(OBJDIR)/HttpRequestHandlerPreflight.o: Http/Handler/HttpRequestHandler.hpp Http/Protocol/HttpFraming.hpp Http/Protocol/RoutePolicy.hpp utils/StringUtils.hpp
$(OBJDIR)/HttpRequestHandlerResponse.o: Http/Handler/HttpRequestHandler.hpp utils/FileUtils.hpp
$(OBJDIR)/HttpRequestHandlerCgi.o: Http/Handler/HttpRequestHandler.hpp Http/Message/HttpRequest.hpp Http/Protocol/HttpParser.hpp Http/Handler/CgiHandler.hpp Network_Server/Connectionslot.hpp utils/FileUtils.hpp utils/StringUtils.hpp
$(OBJDIR)/RoutePolicy.o: Http/Protocol/RoutePolicy.hpp Router/Router.hpp
$(OBJDIR)/RoutePath.o: Http/Protocol/RoutePath.hpp Router/Router.hpp Http/Message/HttpRequest.hpp utils/FileUtils.hpp
$(OBJDIR)/StaticHandler.o: Http/Handler/StaticHandler.hpp Http/Message/HttpRequest.hpp Http/Protocol/RoutePath.hpp utils/FileUtils.hpp
$(OBJDIR)/CgiHandler.o:    Http/Handler/CgiHandler.hpp Cgi/CgiSession.hpp Cgi/CgiProcess.hpp Http/Message/HttpRequest.hpp Http/Protocol/RoutePath.hpp utils/FileUtils.hpp utils/StringUtils.hpp
$(OBJDIR)/CgiProcess.o:    Cgi/CgiProcess.hpp
$(OBJDIR)/CgiEnv.o:        Cgi/CgiEnv.hpp Http/Message/HttpRequest.hpp utils/StringUtils.hpp

.PHONY: all clean fclean re
