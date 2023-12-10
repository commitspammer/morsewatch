package main

import (
	"io"
	"bytes"
	"fmt"
	//"errors"
	"net/http"
	"github.com/gin-gonic/gin"
	//"github.com/gin-gonic/cors"
)

type Event struct {
	Id string `form:"id" json:"id" binding:"required"`
	Timestamp string `form:"timestamp" json:"timestamp" binding:"required"`
	Message string `form:"message" json:"message" binding:"required"`
}
type Stream chan Event
var streams = make(map[Stream]bool)

func main() {
	router := gin.Default()
	router.StaticFile("/", "./index.html")
	router.StaticFile("/favicon.ico", "./favicon.ico")
	//config := cors.DefaultConfig()
	//config.AllowAllOrigins = true
	//config.AllowHeaders = []string{"Origin", "Authorization", "Content-Type", "Accept"}
	//router.Use(cors.New(config))
	router.Use(ErrorHandler)
	router.Use(func (c *gin.Context) {
		body, _ := io.ReadAll(c.Request.Body)
		println(string(body))
		c.Request.Body = io.NopCloser(bytes.NewReader(body))
		c.Next()
	})
	router.GET("/events/listen", StreamHandler, HandleEventListen)
	router.POST("/events/emit", HandleEventEmit)
	router.Run(":8080")
}

func StreamHandler(c *gin.Context) {
	s := make(Stream)
	streams[s] = true
	c.Set("stream", s)
	defer func() { //since C.Stream() is keep-alive, this runs when connection closes
		delete(streams, s)
		close(s)
	}()
	c.Next()
}

func HandleEventListen(c *gin.Context) {
	tmp, ok := c.Get("stream")
	if !ok {
		return
	}
	s, ok := tmp.(Stream)
	if !ok {
		return
	}
	c.Stream(func(w io.Writer) bool {
		if event, ok := <-s; ok {
			c.SSEvent("message", event)
			return true
		} else {
			return false
		}
	})
}

func HandleEventEmit(c *gin.Context) {
	var event Event
	if err := c.ShouldBind(&event); err != nil {
		print(err.Error())
		c.AbortWithError(http.StatusBadRequest, err)
		return
	}
	for s, _ := range streams {
		s <- event //if by chance it tries to send to a dead stream, it's over
	}
	c.Status(http.StatusNoContent)
}

func ErrorHandler(c *gin.Context) {
	c.Next()
	if c.IsAborted() {
		if len(c.Errors) > 0 {
			c.JSON(-1, gin.H{ "message": c.Errors[len(c.Errors)-1].Err.Error() })
		}
	} else {
		for _, err := range c.Errors {
			fmt.Println("error: " + err.Err.Error())
		}
	}
}
