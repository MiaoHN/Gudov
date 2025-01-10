from locust import HttpUser, task, between

class EchoServerUser(HttpUser):
    # 每个用户执行任务之间的等待时间在 1 到 5 秒之间
    wait_time = between(1, 5)  

    @task
    def echo_request(self):
        # 这里假设 EchoServer 的 echo 接口路径为 /echo，你需要根据实际情况修改
        response = self.client.get("/echo")  
        # 可以根据需要添加更多的断言或处理逻辑，比如检查响应状态码等
        if response.status_code!= 200:
            print(f"Received non-200 status code: {response.status_code}")