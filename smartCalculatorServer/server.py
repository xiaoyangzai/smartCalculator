#!/usr/bin/python
# -*- coding: utf-8 -*-

import sys
import tensorflow as tf
import socket
import struct
import numpy as np
import cv2

def load_trained_model(meta_file,meta_path,sess):
    saver = tf.train.import_meta_graph(meta_file)
    saver.restore(sess,tf.train.latest_checkpoint(meta_path))
    graph = tf.get_default_graph()
    inputs = graph.get_operation_by_name('inputs').outputs[0]
    labels = graph.get_operation_by_name('labels').outputs[0]
    accuracy = tf.get_collection("predict_network")[0]
    classes = tf.get_collection("classes")[0]
    probs = tf.get_collection("probs")[0]
    print "Model has initilized successfully!"
    return classes,probs,inputs

def init_server(IP,port):
    sockfd = socket.socket(socket.AF_INET,socket.SOCK_STREAM)
    sockfd.bind((IP,port))
    sockfd.listen(5)
    sockfd.setsockopt(socket.SOL_SOCKET,socket.SO_REUSEADDR,1)
    print "socket has initilizes successfully!"
    return sockfd


def main():
    if len(sys.argv) != 4:
        print "usage ./server [meta file] [meta path] [port]"
        return

    sess = tf.Session()
    classes,probs,inputs = load_trained_model(sys.argv[1],sys.argv[2],sess)
    sockfd = init_server("0.0.0.0",int(sys.argv[3]))


    print "waiting for client....."
    conn,address = sockfd.accept()
    print "A new connection is coming: "
    print "clint Information: ",address

    data = conn.recv(struct.calcsize("!BBi")) 
    decode_data = struct.unpack_from("!BBi",data)
    print "recv: ",decode_data

    img = conn.recv(decode_data[2], socket.MSG_WAITALL)
    #recv默认将字节流转换为字符串类型,因此需要struct.unpack进行转换
    newimg = struct.unpack('B'*decode_data[2],img)
    newimg = np.array(newimg).reshape(224,224,3)

    #使用模型进行预测
    cls,prbs = sess.run([classes,probs],feed_dict={inputs:[newimg]})
    print cls
    print prbs

    #返回预测结果,length中保存价格
    ret = struct.pack("!BBi",0xEF,cls[0],0)
    conn.sendall(ret)
    conn.close()

if __name__ == "__main__":
    main()
