#!/usr/bin/guile \
-s
!#

;;; ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; better list-processing stuff
(use-modules (srfi srfi-1))

;;; better printing
(use-modules (ice-9 pretty-print))
(use-modules (ice-9 format))

;;; and finally the xbindjoy library
(use-modules (saulrh xbindjoy))

;;; ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; helper functions
(define (display-n x)
  (display x) (newline))

;;; ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; first things first: find our joystick and initialize bindings for it

;; (define jsname "DragonRise Inc.   Generic   USB  Joystick  ")
;; (define jsd (jsname->device jsname))
;; (if (string? jsd) 
;;     (display-n (string-append "found joystick" jsd))
;;     (begin
;;       (format #t "Couldn't find joystick ~s\n" jsname)
;;       (quit)))
(define jsd "/dev/input/js0")

(define naxes (get-js-num-axes jsd))
(define nbuttons (get-js-num-buttons jsd))
(init-xbindjoy nbuttons naxes)

;;; ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; assign symbols to buttons. 
;;; 
;;; if you get a new joystick and don't want to change your code too mcuh, you can change these
;;; around to match the joystick easily

;;; axes
(define ax-lx 0)                        ;left stick x axis
(define ax-ly 1)                        ;left stick y axis
(define ax-rx 3)                        ;right stick x axis
(define ax-ry 4)                        ;right stick y axis
(define ax-dx 5)                        ;d-pad x axis
(define ax-dy 6)                        ;d-pad y axis

(if (< naxes 5)
    (format #t
"ERROR: AXISCOUNT: Your joystick has fewer axes than the one I used to build this example (~a vs
~a). You will probably have to go into the example and fiddle with the axis numbering
information.\n" 5 naxes))

;;;
(define bt-a 2)
(define bt-b 1)
(define bt-x 3)
(define bt-y 0)
(define bt-start 9)
(define bt-sel 8)

;;; bumpers and triggers
(define bt-lb 6)
(define bt-rb 7)
(define bt-tb 4)
(define bt-tb 5)

;; ;;; d-pad ends up as buttons
;; (define bt-dup 8)
;; (define bt-ddown 9)
;; (define bt-dleft 10)
;; (define bt-dright 11)

;;; ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; variables

(define do-axes-display #f)

;;; ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; button callbacks
(bind-button->proc `(press . ,bt-a) (lambda () (display "button a down 1\n")))
(bind-button->proc `(release . ,bt-a) (lambda () (display "button a up 1\n")))

(bind-button->proc `(press . ,bt-a) (lambda () (display "button a down 2\n")))
(bind-button->proc `(release . ,bt-a) (lambda () (display "button a up 2\n")))

(bind-button->proc `(press . ,bt-b) (lambda () (display "button b down\n")))
(bind-button->proc `(release . ,bt-b) (lambda () (display "button b up\n")))


(bind-button->proc `(press . ,bt-x) (lambda () (send-key 'press 'X 0)))
(bind-button->proc `(release . ,bt-x) (lambda () (send-key 'release 'X 0)))

(bind-button->key bt-y 'Y)
(bind-button->mbutton bt-lb 1)

(bind-button->proc `(press . ,bt-start)
                   (lambda () (set! do-axes-display (not do-axes-display))))

(bind-button->proc `(press . ,bt-sel)
                   (let ((seq (text->keyseq "select")))
                     (lambda () 
                       (send-keyseq seq))))


(bind-button->proc `(press . ,bt-rb)
                   (build-send-key-toggler 'Shift_L #f))

;;; ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; axis callbacks

(bind-axis->proc (lambda (dt axes axes-last)
                   (if do-axes-display
                       (begin
                         (display axes)
                         (newline)))))

(bind-axis->proc (lambda (dt axes axes-last)
                   (begin (if (ax-trans? axes axes-last ax-ly 0.7 #t)
                              (send-key 'press 'S 0))
                          (if (ax-trans? axes axes-last ax-ly 0.7 #f)
                              (send-key 'release 'S 0)))))
(bind-axis-region->key ax-ly -0.7 -1.2 'W)
(bind-axis-region->key ax-lx -0.7 -1.2 'A)
(bind-axis-region->key ax-lx  0.7  1.2 'D)

(define mousespeed 100)
(bind-button->proc `(press . ,bt-rb)
                   (lambda () (set! mousespeed 1000)))
(bind-button->proc `(release . ,bt-rb)
                   (lambda () (set! mousespeed 100)))

(bind-axis->proc (lambda (dt axes axes-last)
                   (let* ((lsx (assoc-ref axes ax-rx))
                          (lsy (assoc-ref axes ax-ry))
                          (vx (* lsx mousespeed))
                          (vy (* lsy mousespeed))
                          (dx (* vx dt))
                          (dy (* vy dt)))
                     (send-mouserel dx dy))))

(xbindjoy-start jsd)
(display "\n")
