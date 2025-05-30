# 📦 프로젝트 실행 및 인증서 생성 가이드

## 1. 개발 환경 준비

서버 및 클라이언트 개발에 필요한 라이브러리를 설치하세요.

```bash
sudo apt update
sudo apt install libsqlite3-dev  # SQLite 동작에 필요
sudo apt install libssl-dev      # OpenSSL 개발 라이브러리
```

## 2. 서버측 인증서 생성

⚠️ **아래 과정은 반드시 `server/` 폴더에서 진행해야 합니다.**

---

### 2-1. 자체 CA(인증기관) 인증서 만들기

#### 📌 CA 개인키 생성

```bash
openssl genpkey -algorithm RSA -out ca.key -aes256
```
비밀번호를 입력하라는 메시지가 나오면 안전하게 입력하세요.

#### 📌 CA 인증서 생성
```bash
openssl req -x509 -new -nodes -key ca.key -sha256 -days 3650 -out ca.crt
```
국가, 지역, 조직 등 정보를 입력하라는 메시지가 나오면 적절히 입력하세요.

---
### 2-2. 서버 인증서 만들기

#### 📌 서버 개인키 생성
```bash
openssl genpkey -algorithm RSA -out server.key
```
#### 📌 서버 인증서 서명 요청 (CSR) 생성
```bash
openssl req -new -key server.key -out server.csr
```
#### 📌 CA로 서버 인증서 서명
```bash
openssl x509 -req -in server.csr -CA ca.crt -CAkey ca.key -CAcreateserial -out server.crt -days 3650 -sha256
```

## 3. 클라이언트 인증서 준비
서버 폴더에서 생성한 ca.crt 파일을 client/ 폴더에 복사하세요
```bash
cp server/ca.crt client/
```
클라이언트는 이 인증서를 통해 서버의 인증서를 검증합니다.

## 4. 실행 방법
#### 4-1. 서버 실행
```bash
cd server
make
./server
```
#### 4-2. 클라이언트 실행
```bash
cd client
make
./client
```
